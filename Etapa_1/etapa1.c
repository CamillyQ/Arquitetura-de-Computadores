#include <stdio.h> 
#include <stdlib.h> 
#include <stdint.h> 
#include <string.h> 
#include <ctype.h> 

#define DADOS_FILE "dados_etapa3_tarefa1.txt" 
#define MICRO_FILE "microinstrucoes_etapa3_tarefa1.txt" 
#define REGS_FILE "registradores_etapa3_tarefa1.txt" 
#define MICRO_FILE_ALT "microinstrucoes_etapa3_tarefa1" 
#define IJVM_FILE "instrucoes_ijvm.txt" 
#define LOG_FILE "log.txt" 
#define MAX_LINE 512 
#define MEM_LINES 16 

typedef struct { 
    uint32_t H, OPC, TOS, CPP, LV, SP, PC, MDR, MAR; 
    uint8_t MBR; 
} Regs; 

Regs R; 
uint32_t MEM[MEM_LINES]; 
int cycle_number = 1; 

/* utils */ 
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

/* conversores */ 
uint32_t bin32_to_u32(const char *s) { 
    uint32_t v=0; 
    for(size_t i=0;s[i];i++) 
        if(s[i]=='0'||s[i]=='1') v=(v<<1)|(s[i]-'0'); 
    return v; 
} 

uint8_t bin_to_u8(const char *s) { 
    uint8_t v=0; 
    for(size_t i=0;s[i];i++) 
        if(s[i]=='0'||s[i]=='1') v=(v<<1)|(s[i]-'0'); 
    return v; 
} 

char *read_line_trim(FILE *f, char *buf, size_t n) { 
    if (!fgets(buf,(int)n,f)) return NULL; 
    size_t len=strlen(buf); 
    while(len>0&&(buf[len-1]=='\n'||buf[len-1]=='\r')) buf[--len]='\0'; 
    return buf; 
} 

/* pega parte depois do '=' */ 
char* after_equal(char *s) { 
    char *p=strchr(s,'='); 
    if(p) { 
        p++; 
        while(*p && isspace((unsigned char)*p)) p++; 
        return p; 
    } 
    return s; 
} 

/* decode B */ 
uint32_t get_B_value_from_decoder(int code) { 
    switch (code) { 
        case 8: return R.OPC; 
        case 7: return R.TOS; 
        case 6: return R.CPP; 
        case 5: return R.LV; 
        case 4: return R.SP; 
        case 3: return (uint32_t)R.MBR; 
        case 2: { 
            uint8_t b=R.MBR; 
            return (b&0x80)?0xFFFFFF00U|b:(uint32_t)b; 
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

/* escreve Sd nos registradores habilitados */ 
void write_C_destination(uint16_t sel9, uint32_t Sd) { 
    if (sel9&(1<<8)) R.H=Sd; 
    if (sel9&(1<<7)) R.OPC=Sd; 
    if (sel9&(1<<6)) R.TOS=Sd; 
    if (sel9&(1<<5)) R.CPP=Sd; 
    if (sel9&(1<<4)) R.LV=Sd; 
    if (sel9&(1<<3)) R.SP=Sd; 
    if (sel9&(1<<2)) R.PC=Sd; 
    if (sel9&(1<<1)) R.MDR=Sd; 
    if (sel9&(1<<0)) R.MAR=Sd; 
} 

/* ULA */ 
typedef struct { 
    uint32_t Sd; 
    int N; 
    int Z; 
} ULA_out; 

ULA_out execute_ula(uint8_t c, uint32_t A, uint32_t B) { 
    int SLL8=(c>>7)&1,SRA1=(c>>6)&1,F0=(c>>5)&1,F1=(c>>4)&1; 
    int ENA=(c>>3)&1,ENB=(c>>2)&1,INVA=(c>>1)&1,INC=c&1; 
    uint32_t a=ENA?A:0,b=ENB?B:0; 
    if(INVA) a=~a; 
    uint32_t Sd=0; 
    int code=(F0<<1)|F1; 
    if(code==0) Sd=b; 
    else if(code==1) Sd=a; 
    else if(code==2) Sd=a&b; 
    else Sd=a+b+INC; 
    if(SLL8) Sd<<=8; 
    else if(SRA1) Sd=(uint32_t)((int32_t)Sd>>1); 
    ULA_out o={Sd,((int32_t)Sd<0),Sd==0}; 
    return o; 
} 

void format_ir_spaced(const char *m,char*out){ 
    int pos=0;for(int i=0;i<8;i++)out[pos++]=m[i];out[pos++]=' '; 
    for(int i=8;i<17;i++)out[pos++]=m[i];out[pos++]=' '; 
    out[pos++]=m[17];out[pos++]=m[18];out[pos++]=' '; 
    for(int i=19;i<23;i++)out[pos++]=m[i];out[pos]=0; 
} 

/* executar microinstr */ 
int execute_microinstr(const char *m) { 
    if(!m||strlen(m)<23) return 0; 
    uint8_t ula=0;for(int i=0;i<8;i++) ula=(ula<<1)|(m[i]=='1'); 
    uint16_t selC=0;for(int i=8;i<17;i++) selC=(selC<<1)|(m[i]=='1'); 
    int WRITE=(m[17]=='1'),READ=(m[18]=='1'); 
    int Bcode=0;for(int i=19;i<23;i++) Bcode=(Bcode<<1)|(m[i]=='1'); 
    Regs before=R; 

    // 1. Executa a lógica da ULA e atualiza os registradores primeiro
    uint32_t Bval=get_B_value_from_decoder(Bcode); 
    ULA_out uout=execute_ula(ula,before.H,Bval); // Usa o H de 'before'
    write_C_destination(selC,uout.Sd); 

    // 2. CORREÇÃO: Executa as operações de memória DEPOIS da ULA,
    // usando os valores atualizados dos registradores
    if (READ && R.MAR < MEM_LINES) {
        R.MDR = MEM[R.MAR];
    }
    if (WRITE && R.MAR < MEM_LINES) {
        MEM[R.MAR] = R.MDR;
    }

    FILE*log=fopen(LOG_FILE,"a");if(!log)return 0; 
    fprintf(log,"Cycle %d\n",cycle_number++); 
    char ir[32];format_ir_spaced(m,ir);fprintf(log,"ir = %s\n",ir); 
    fprintf(log,"b = %s\n",get_B_name(Bcode)); 
    fprintf(log,"c ="); 
    int f=1; 
    if(selC&(1<<0)){if(!f)fputc(' ',log);fprintf(log,"mar");f=0;}
    if(selC&(1<<1)){if(!f)fputc(' ',log);fprintf(log,"mdr");f=0;} 
    if(selC&(1<<2)){if(!f)fputc(' ',log);fprintf(log,"pc");f=0;} 
    if(selC&(1<<3)){if(!f)fputc(' ',log);fprintf(log,"sp");f=0;} 
    if(selC&(1<<4)){if(!f)fputc(' ',log);fprintf(log,"lv");f=0;} 
    if(selC&(1<<5)){if(!f)fputc(' ',log);fprintf(log,"cpp");f=0;} 
    if(selC&(1<<6)){if(!f)fputc(' ',log);fprintf(log,"tos");f=0;} 
    if(selC&(1<<7)){if(!f)fputc(' ',log);fprintf(log,"opc");f=0;} 
    if(selC&(1<<8)){if(!f)fputc(' ',log);fprintf(log,"h");f=0;} 
    fprintf(log,"\n\n"); 
    fprintf(log,"> Registers before instruction\n*******************************\n");dump_regs(log,&before); 
    fprintf(log,"\n> Registers after instruction\n*******************************\n");dump_regs(log,&R); 
    fprintf(log,"\n> Memory after instruction\n*******************************\n"); 
    for(int i=0;i<MEM_LINES;i++) print_bin32(log,MEM[i]); 
    fprintf(log,"============================================================\n"); 
    fclose(log);return 1; 
}

/* carregar registradores */ 
int load_initial_regs(const char *fname) { 
    FILE *f=fopen(fname,"r");if(!f)return 0; 
    char buf[MAX_LINE]; 
    read_line_trim(f,buf,MAX_LINE);R.MAR=bin32_to_u32(after_equal(buf)); 
    read_line_trim(f,buf,MAX_LINE);R.MDR=bin32_to_u32(after_equal(buf)); 
    read_line_trim(f,buf,MAX_LINE);R.PC =bin32_to_u32(after_equal(buf)); 
    read_line_trim(f,buf,MAX_LINE);R.MBR=bin_to_u8(after_equal(buf)); 
    read_line_trim(f,buf,MAX_LINE);R.SP =bin32_to_u32(after_equal(buf)); 
    read_line_trim(f,buf,MAX_LINE);R.LV =bin32_to_u32(after_equal(buf)); 
    read_line_trim(f,buf,MAX_LINE);R.CPP=bin32_to_u32(after_equal(buf)); 
    read_line_trim(f,buf,MAX_LINE);R.TOS=bin32_to_u32(after_equal(buf)); 
    read_line_trim(f,buf,MAX_LINE);R.OPC=bin32_to_u32(after_equal(buf)); 
    read_line_trim(f,buf,MAX_LINE);R.H =bin32_to_u32(after_equal(buf)); 
    fclose(f);return 1; 
} 

int load_mem(const char*fname){FILE*f=fopen(fname,"r");if(!f)return 0; char buf[MAX_LINE];int i=0; while(i<MEM_LINES&&read_line_trim(f,buf,MAX_LINE)){if(strlen(buf)==0)continue;MEM[i++]=bin32_to_u32(buf);} for(;i<MEM_LINES;i++)MEM[i]=0;fclose(f);return 1;} 

int run_micro_file(const char*fname){FILE*f=fopen(fname,"r");if(!f)return 0; char buf[MAX_LINE];int ex=0; while(read_line_trim(f,buf,MAX_LINE)){if(strlen(buf)==0)continue; char m[32];int idx=0;for(size_t i=0;i<strlen(buf)&&idx<23;i++)if(buf[i]=='0'||buf[i]=='1')m[idx++]=buf[i]; m[idx]=0;if(idx==23){if(execute_microinstr(m))ex++;}} fclose(f);return ex;} 

int main(){ 
    if(!load_initial_regs(REGS_FILE)){fprintf(stderr,"Erro regs\n");return 1;} 
    if(!load_mem(DADOS_FILE)){fprintf(stderr,"Erro mem\n");return 1;} 
    FILE*log=fopen(LOG_FILE,"w"); 
    fprintf(log,"============================================================\n"); 
    fprintf(log,"Initial memory state\n*******************************\n"); 
    for(int i=0;i<MEM_LINES;i++)print_bin32(log,MEM[i]); 
    fprintf(log,"*******************************\nInitial register state\n*******************************\n"); 
    dump_regs(log,&R); 
    fprintf(log,"============================================================\nStart of Program\n============================================================\n"); 
    fclose(log); 
    int ex=run_micro_file(MICRO_FILE);if(!ex)ex=run_micro_file(MICRO_FILE_ALT); 
    log=fopen(LOG_FILE,"a");fprintf(log,"Cycle %d\n",cycle_number);fprintf(log,"No more lines, EOP.\n");fclose(log); 
    return 0; 
}