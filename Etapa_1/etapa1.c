#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define DADOS_FILE "dados_etapa3_tarefa1.txt"
#define REGS_FILE "registradores_etapa3_tarefa1.txt"
#define IJVM_FILE "instrucoes_ijvm.txt"
#define MICRO_FILE "microinstrucoes_etapa3_tarefa1.txt" // Arquivo para teste de microinstruções
#define LOG_FILE "log.txt"
#define MAX_LINE 512
#define MEM_LINES 16

typedef struct {
    uint32_t H, OPC, TOS, CPP, LV, SP, PC, MDR, MAR;
    uint8_t MBR;
} Regs;

Regs R;
uint32_t MEM[MEM_LINES];
int cycle_number = 0; /* começar em 0: será incrementado 1x por instrução IJVM */

/* =========================================================
   Utils de impressão
   ========================================================= */
void print_bin32(FILE *log, uint32_t v) {
    for (int i = 31; i >= 0; i--)
        fputc(((v >> i) & 1) ? '1' : '0', log);
    fputc('\n', log);
}

void print_bin8(FILE *log, uint8_t v) {
    for (int i = 7; i >= 0; i--)
        fputc(((v >> i) & 1) ? '1' : '0', log);
    fputc('\n', log);
}

void dump_regs(FILE *log, Regs *r) {
    fprintf(log, "mar = "); print_bin32(log, r->MAR);
    fprintf(log, "mdr = "); print_bin32(log, r->MDR);
    fprintf(log, "pc = ");  print_bin32(log, r->PC);
    fprintf(log, "mbr = "); print_bin8(log, r->MBR);
    fprintf(log, "sp = ");  print_bin32(log, r->SP);
    fprintf(log, "lv = ");  print_bin32(log, r->LV);
    fprintf(log, "cpp = "); print_bin32(log, r->CPP);
    fprintf(log, "tos = "); print_bin32(log, r->TOS);
    fprintf(log, "opc = "); print_bin32(log, r->OPC);
    fprintf(log, "h = ");   print_bin32(log, r->H);
}

/* =========================================================
   Conversores
   ========================================================= */
uint32_t bin32_to_u32(const char *s) {
    uint32_t v = 0;
    for (size_t i = 0; s[i]; i++)
        if (s[i] == '0' || s[i] == '1') v = (v << 1) | (s[i] - '0');
    return v;
}

uint8_t bin_to_u8(const char *s) {
    uint8_t v = 0;
    for (size_t i = 0; s[i]; i++)
        if (s[i] == '0' || s[i] == '1') v = (v << 1) | (s[i] - '0');
    return v;
}

char *read_line_trim(FILE *f, char *buf, size_t n) {
    if (!fgets(buf, (int)n, f)) return NULL;
    size_t len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) buf[--len] = '\0';
    return buf;
}

/* parse byte: aceita binário puro ou decimal/hex */
static unsigned int parse_byte_str(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return 0;
    int only01 = 1;
    for (const char *p = s; *p; ++p) if (*p!='0' && *p!='1') { only01 = 0; break; }
    if (only01) {
        unsigned int v = 0;
        for (const char *p = s; *p; ++p) v = (v << 1) | ((*p=='1')?1u:0u);
        return v & 0xFFu;
    } else {
        return (unsigned int)strtoul(s, NULL, 0) & 0xFFu;
    }
}

/* =========================================================
   Decodificadores B e C
   ========================================================= */
uint32_t get_B_value_from_decoder(int code) {
    switch (code) {
        case 8: return R.OPC;
        case 7: return R.TOS;
        case 6: return R.CPP;
        case 5: return R.LV;
        case 4: return R.SP;
        case 3: return (uint32_t)R.MBR; // MBRU
        case 2: { // MBR (sign-extend)
            uint8_t b = R.MBR;
            return (b & 0x80) ? 0xFFFFFF00U | b : (uint32_t)b;
        }
        case 1: return R.PC;
        case 0: return R.MDR;
        default: return 0;
    }
}

const char* get_B_name(int code) {
    switch (code) {
        case 8: return "opc";
        case 7: return "tos";
        case 6: return "cpp";
        case 5: return "lv";
        case 4: return "sp";
        case 3: return "mbru";
        case 2: return "mbr";
        case 1: return "pc";
        case 0: return "mdr";
        default: return "?";
    }
}

/* =========================================================
   Escrita no barramento C
   ========================================================= */
void write_C_destination(uint16_t sel9, uint32_t Sd) {
    if (sel9 & (1 << 8)) R.H = Sd;
    if (sel9 & (1 << 7)) R.OPC = Sd;
    if (sel9 & (1 << 6)) R.TOS = Sd;
    if (sel9 & (1 << 5)) R.CPP = Sd;
    if (sel9 & (1 << 4)) R.LV = Sd;
    if (sel9 & (1 << 3)) R.SP = Sd;
    if (sel9 & (1 << 2)) R.PC = Sd;
    if (sel9 & (1 << 1)) R.MDR = Sd;
    if (sel9 & (1 << 0)) R.MAR = Sd;
}

/* =========================================================
   ULA
   ========================================================= */
typedef struct {
    uint32_t Sd;
    int N;
    int Z;
} ULA_out;

ULA_out execute_ula(uint8_t c, uint32_t A, uint32_t B) {
    int SLL8 = (c >> 7) & 1, SRA1 = (c >> 6) & 1, F0 = (c >> 5) & 1, F1 = (c >> 4) & 1;
    int ENA = (c >> 3) & 1, ENB = (c >> 2) & 1, INVA = (c >> 1) & 1, INC = c & 1;
    uint32_t a = ENA ? A : 0, b = ENB ? B : 0;
    if (INVA) a = ~a;
    uint32_t Sd = 0;
    int code = (F0 << 1) | F1;
    if (code == 0) Sd = b;
    else if (code == 1) Sd = a;
    else if (code == 2) Sd = a & b;
    else Sd = a + b + INC;
    if (SLL8) Sd <<= 8;
    else if (SRA1) Sd = (uint32_t)((int32_t)Sd >> 1);
    ULA_out o = {Sd, ((int32_t)Sd < 0), Sd == 0};
    return o;
}

void format_ir_spaced(const char *m, char *out) {
    int pos = 0;
    for (int i = 0; i < 8; i++) out[pos++] = m[i];
    out[pos++] = ' ';
    for (int i = 8; i < 17; i++) out[pos++] = m[i];
    out[pos++] = ' ';
    out[pos++] = m[17]; out[pos++] = m[18]; out[pos++] = ' ';
    for (int i = 19; i < 23; i++) out[pos++] = m[i];
    out[pos] = 0;
}

/* =========================================================
   Execução de uma microinstrução (23 bits)
   ========================================================= */
int execute_microinstr(const char *m) {
     cycle_number++;
    if (!m || strlen(m) < 23) return 0;
    uint8_t ula = 0; for (int i = 0; i < 8; i++) ula = (ula << 1) | (m[i] == '1');
    uint16_t selC = 0; for (int i = 8; i < 17; i++) selC = (selC << 1) | (m[i] == '1');
    int WRITE = (m[17] == '1'), READ = (m[18] == '1');
    int Bcode = 0; for (int i = 19; i < 23; i++) Bcode = (Bcode << 1) | (m[i] == '1');
    Regs before = R;

    uint32_t Bval = get_B_value_from_decoder(Bcode);
    ULA_out uout = execute_ula(ula, before.H, Bval);
    write_C_destination(selC, uout.Sd);

    /* Memória após ULA */
    if (R.MAR < MEM_LINES) {
        if (READ && WRITE) {
            // fetch especial: já tratado no tradutor (R.MBR/H preenchidos)
        } else if (READ) {
            R.MDR = MEM[R.MAR];
        } else if (WRITE) {
            MEM[R.MAR] = R.MDR;
        }
    }

    FILE *log = fopen(LOG_FILE, "a"); if (!log) return 0;
    fprintf(log, "Cycle %d\n", cycle_number);
    char ir[32]; format_ir_spaced(m, ir); fprintf(log, "ir = %s\n", ir);
    fprintf(log, "b = %s\n", get_B_name(Bcode));
   // Lógica para imprimir os registradores do Barramento C
    const char *c_names[] = {"H", "OPC", "TOS", "CPP", "LV", "SP", "PC", "MDR", "MAR"}; // Ordem do bit 8 para o bit 0 [cite: 110]
    fprintf(log, "c =");
    int first_c = 1;
    for (int i = 8; i >= 0; i--) { // Itera pelos 9 bits de controle, do mais significativo ao menos
        if ((selC >> i) & 1) { // Verifica se o bit 'i' está ligado
            if (!first_c) {
                fprintf(log, ","); // Adiciona vírgula se não for o primeiro
            }
            fprintf(log, " %s", c_names[8 - i]); // Imprime o nome do registrador
            first_c = 0;
        }
    }
    if (first_c) {
        fprintf(log, " (nenhum)"); // Caso nenhum registrador seja escrito
    }
    fprintf(log, "\n");
    fprintf(log, "\n> Registers before instruction\n*******************************\n"); dump_regs(log, &before);
    fprintf(log, "\n> Registers after instruction\n*******************************\n"); dump_regs(log, &R);
    fprintf(log, "\n> Memory after instruction\n*******************************\n");
    for (int i = 0; i < MEM_LINES; i++) print_bin32(log, MEM[i]);
    fprintf(log, "============================================================\n");
    fclose(log);
    return 1;
}

/* =========================================================
   Loader de arquivos iniciais
   ========================================================= */
// VERSÃO CORRIGIDA
int load_initial_regs(const char *fname) {
    FILE *f = fopen(fname, "r"); if (!f) return 0;
    char buf[MAX_LINE];
    read_line_trim(f, buf, MAX_LINE); R.MAR = bin32_to_u32(strchr(buf,'=')+1);
    read_line_trim(f, buf, MAX_LINE); R.MDR = bin32_to_u32(strchr(buf,'=')+1);
    read_line_trim(f, buf, MAX_LINE); R.PC  = bin32_to_u32(strchr(buf,'=')+1);
    read_line_trim(f, buf, MAX_LINE); R.MBR = bin_to_u8(strchr(buf,'=')+1); // Usa bin_to_u8 para 8 bits
    read_line_trim(f, buf, MAX_LINE); R.SP  = bin32_to_u32(strchr(buf,'=')+1);
    read_line_trim(f, buf, MAX_LINE); R.LV  = bin32_to_u32(strchr(buf,'=')+1);
    read_line_trim(f, buf, MAX_LINE); R.CPP = bin32_to_u32(strchr(buf,'=')+1);
    read_line_trim(f, buf, MAX_LINE); R.TOS = bin32_to_u32(strchr(buf,'=')+1);
    read_line_trim(f, buf, MAX_LINE); R.OPC = bin32_to_u32(strchr(buf,'=')+1);
    read_line_trim(f, buf, MAX_LINE); R.H   = bin32_to_u32(strchr(buf,'=')+1);
    fclose(f); return 1;
}

int load_mem(const char*fname){
    FILE*f=fopen(fname,"r"); if(!f) return 0;
    char buf[MAX_LINE]; int i=0;
    while(i<MEM_LINES && read_line_trim(f,buf,MAX_LINE)){
        if(strlen(buf)==0) continue;
        MEM[i++]=bin32_to_u32(buf);
    }
    for(;i<MEM_LINES;i++) MEM[i]=0;
    fclose(f); return 1;
}

/* =========================================================
   Tradução IJVM -> microinstruções
   ========================================================= */
int generate_micro_ILOAD(int x){
    /* assume-se verificação prévia no run_ijvm_file */
    execute_microinstr("00110000010000000010100"); // H = LV
    for(int i=0;i<x;i++)
        execute_microinstr("00110000010000000011100"); // H = H+1
    execute_microinstr("00111000100000000010000"); // MAR = H; rd
    execute_microinstr("00110101000001001000100"); // MAR = SP=SP+1
    
    // ## CORREÇÃO ## A ULA "00111000..." lia H. A correta "00000100..." passa B (MDR)
    execute_microinstr("00000100001000010100000"); // TOS = MDR; wr
    return 1;
}

void generate_micro_DUP(){
    execute_microinstr("00110101000001001000100"); // MAR = SP=SP+1

    // ## CORREÇÃO ## A ULA "00010100..." resultava em 0. A correta "00000100..." passa B (TOS)
    execute_microinstr("00000100000000010100111"); // MDR = TOS; wr
}

void generate_micro_BIPUSH(uint8_t val){
    execute_microinstr("00110101000001001000100"); // SP=MAR=SP+1
    char micro[24];
    for (int i = 7; i >= 0; i--) micro[7-i] = ((val >> i) & 1) ? '1' : '0';
    strcpy(micro+8, "000000000110000");
    micro[23] = '\0';
    R.MBR = val;
    R.H = val;
    execute_microinstr(micro);
    execute_microinstr("00111000001000010100000"); // MDR=TOS=H; wr
}

/* =========================================================
   Executor de IJVM
   ========================================================= */
int run_ijvm_file(const char *fname){
    FILE*f=fopen(fname,"r"); if(!f) return 0;
    char buf[MAX_LINE];
    while(read_line_trim(f,buf,MAX_LINE)){
        if(strlen(buf)==0) continue;
        FILE *out = fopen(LOG_FILE, "a");
        if (out) {
            fprintf(out, "Executando: %s\n", buf);
            fclose(out);
        }

        if(strncmp(buf,"ILOAD",5)==0){
            int x = atoi(buf+6);
            /* Verifica condição de acesso antes de incrementar ciclo */
            if ((uint32_t)(R.LV + x) > R.SP) {
                FILE *out2 = fopen(LOG_FILE, "a");
                if (out2) {
                    fprintf(out2, "========================================================\n");
                    fprintf(out2, "!!!! ERRO: ILOAD com endereco acima do topo da pilha: LV(%u) + %d > SP(%u) !!!!\n",
                            (unsigned)R.LV, x, (unsigned)R.SP);
                    fprintf(out2, "========================================================\n");
                    fclose(out2);
                }
                /* não incrementa cycle_number e não avança PC */
            } else {                /* 1 ciclo por instrução IJVM */
                generate_micro_ILOAD(x);
                R.PC = R.PC + 1;
            }
        } else if(strncmp(buf,"DUP",3)==0){
            generate_micro_DUP();
            R.PC = R.PC + 1;
        } else if(strncmp(buf,"BIPUSH",6)==0){
            const char *arg = buf+7;
            unsigned int val = parse_byte_str(arg);
            generate_micro_BIPUSH((uint8_t)val);
            R.PC = R.PC + 1;  /* BIPUSH ocupa opcode + argumento */
        }
    }
    fclose(f); return 1;
}

/* =========================================================
   ## NOVO ## Executor de Microinstruções
   ========================================================= */
void run_micro_file(const char *fname){
    FILE*f=fopen(fname,"r"); 
    if(!f) {
        fprintf(stderr, "Erro ao abrir o arquivo de microinstruções: %s\n", fname);
        return;
    }
    
    char buf[MAX_LINE];
    
    while(read_line_trim(f, buf, MAX_LINE)){
        if(strlen(buf) == 0) continue;
        
        execute_microinstr(buf);
    }
    
    fclose(f);
}


/* =========================================================
   MAIN
   ========================================================= */
int main(){
    if(!load_initial_regs(REGS_FILE)){fprintf(stderr,"Erro regs\n");return 1;}
    if(!load_mem(DADOS_FILE)){fprintf(stderr,"Erro mem\n");return 1;}
    FILE*log=fopen(LOG_FILE,"w");
    if (log) {
        fprintf(log,"============================================================\n"
                    "Initial memory state\n*******************************\n");
        for(int i=0;i<MEM_LINES;i++)print_bin32(log,MEM[i]);
        fprintf(log,"*******************************\nInitial register state\n*******************************\n");
        dump_regs(log,&R);
        fprintf(log,"============================================================\n"
                    "Start of Program\n"
                    "============================================================\n");
        fclose(log);
    }

    // Para testar a Etapa 3, Tarefa 1 (microinstruções), use esta linha:
    run_micro_file(MICRO_FILE);

    // Para testar a execução de um programa IJVM completo, comente a linha acima e descomente a linha abaixo:
    //run_ijvm_file(IJVM_FILE);

    log=fopen(LOG_FILE,"a");
    if (log) {
        cycle_number++;
        fprintf(log,"Cycle %d\n",cycle_number);
        fprintf(log,"No more lines, EOP.\n");
        fprintf(log,"Final PC = "); print_bin32(log, R.PC);
        fclose(log);
    }
    return 0;
}