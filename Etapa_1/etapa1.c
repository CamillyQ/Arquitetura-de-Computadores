#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

/* -------------------- Tipos -------------------- */
typedef struct {
    uint32_t S;     // saída antes do deslocamento
    uint32_t Sd;    // saída final
    int carry;      // vai-um
    int N;          // negativo (bit 31)
    int Z;          // zero
    int error;      // erro de sinais de controle
} ULAOut;

/* -------------------- Registradores -------------------- */
typedef struct {
    uint32_t MAR, MDR, PC;
    uint8_t  MBR;
    uint32_t SP, LV, CPP, TOS, OPC, H;
} Regs;

Regs regs;

/* -------------------- utilitários binários -------------------- */
void to_bin32(uint32_t x, char *out) {
    for (int i = 31; i >= 0; --i) out[31 - i] = ((x >> i) & 1) ? '1' : '0';
    out[32] = '\0';
}
void to_bin8(uint8_t x, char *out) {
    for (int i = 7; i >= 0; --i) out[7 - i] = ((x >> i) & 1) ? '1' : '0';
    out[8] = '\0';
}
uint32_t parse_bin32(const char *s) {
    uint32_t v = 0;
    for (int i = 0; s[i] && (s[i] == '0' || s[i] == '1'); ++i) {
        v = (v << 1) | (s[i] - '0');
    }
    return v;
}
uint8_t parse_bin8(const char *s) {
    uint8_t v = 0;
    for (int i = 0; s[i] && (s[i] == '0' || s[i] == '1'); ++i) {
        v = (uint8_t)((v << 1) | (s[i] - '0'));
    }
    return v;
}

/* -------------------- Função ULA (seu núcleo) -------------------- */
ULAOut ula_exec(uint32_t A, uint32_t B, unsigned char control) {
    ULAOut out = {0};

    int SLL8 = (control >> 7) & 1;
    int SRA1 = (control >> 6) & 1;
    int F0   = (control >> 5) & 1;
    int F1   = (control >> 4) & 1;
    int ENA  = (control >> 3) & 1;
    int ENB  = (control >> 2) & 1;
    int INVA = (control >> 1) & 1;
    int INC  = (control >> 0) & 1;

    if (SLL8 && SRA1) {
        out.error = 1;
        return out;
    }

    uint32_t a = ENA ? A : 0;
    uint32_t b = ENB ? B : 0;
    if (INVA) a = ~a;

    int op = (F0 << 1) | F1;

    if (op == 2) { // soma
        uint64_t sum = (uint64_t)a + (uint64_t)b + (INC ? 1u : 0u);
        out.S = (uint32_t)sum;
        out.carry = (int)((sum >> 32) & 1u);
    } else {
        switch (op) {
            case 0: out.S = a & b; break;
            case 1: out.S = a | b; break;
            case 3: out.S = a ^ b; break;
            default: out.S = 0; break;
        }
        if (INC) out.S = out.S + 1u;
        out.carry = 0;
    }

    out.Sd = out.S;
    if (SLL8) out.Sd = (out.S << 8);
    else if (SRA1) {
        int32_t t = (int32_t)out.S;
        out.Sd = (uint32_t)(t >> 1);
    }

    out.N = (int)((out.Sd >> 31) & 1u);
    out.Z = (out.Sd == 0);

    return out;
}

/* -------------------- Decodificador B (4 bits -> reg) --------------------
   Convenção de nomes para impressão:
   8 OPC,7 TOS,6 CPP,5 LV,4 SP,3 MBRU,2 MBR,1 PC,0 MDR
   Atenção: para reproduzir exatamente o log que você mandou,
   estou usando MBR (code 2) como ZERO-EXTEND e MBRU (code 3) como SIGN-EXTEND,
   que é o comportamento observado no seu exemplo.
   Se quiser que eu volte ao comportamento do enunciado, aviso e altero.
---------------------------------------------------------------------*/
uint32_t get_B_value_and_name(int code, char *name_out) {
    switch (code) {
        case 8: strcpy(name_out, "opc");  return regs.OPC;
        case 7: strcpy(name_out, "tos");  return regs.TOS;
        case 6: strcpy(name_out, "cpp");  return regs.CPP;
        case 5: strcpy(name_out, "lv");   return regs.LV;
        case 4: strcpy(name_out, "sp");   return regs.SP;
        case 3: /* MBRU */ strcpy(name_out, "mbru");
                { /* SIGN-EXTEND (note: isto reproduz seu exemplo) */
                    int8_t s = (int8_t)regs.MBR;
                    return (uint32_t)(int32_t)s;
                }
        case 2: /* MBR  */ strcpy(name_out, "mbr");
                { /* ZERO-EXTEND (note: isto reproduz seu exemplo) */
                    return (uint32_t)regs.MBR;
                }
        case 1: strcpy(name_out, "pc");   return regs.PC;
        case 0: strcpy(name_out, "mdr");  return regs.MDR;
        default: strcpy(name_out, "none"); return 0;
    }
}

/* -------------------- Escrita C (9 bits -> vários regs) --------------------
   Bits: 8 H,7 OPC,6 TOS,5 CPP,4 LV,3 SP,2 PC,1 MDR,0 MAR
---------------------------------------------------------------------*/
void write_C_regs(int mask9, uint32_t value) {
    if (mask9 & (1 << 8)) regs.H   = value;
    if (mask9 & (1 << 7)) regs.OPC = value;
    if (mask9 & (1 << 6)) regs.TOS = value;
    if (mask9 & (1 << 5)) regs.CPP = value;
    if (mask9 & (1 << 4)) regs.LV  = value;
    if (mask9 & (1 << 3)) regs.SP  = value;
    if (mask9 & (1 << 2)) regs.PC  = value;
    if (mask9 & (1 << 1)) regs.MDR = value;
    if (mask9 & (1 << 0)) regs.MAR = value;
}

/* -------------------- Nomes do barramento C (para imprimir c_bus) ------ */
void c_bus_names(int mask9, char *out) {
    /* order: H, OPC, TOS, CPP, LV, SP, PC, MDR, MAR */
    int first = 1;
    out[0] = '\0';
    if (mask9 & (1 << 8)) { strcat(out, "h"); first = 0; }
    if (mask9 & (1 << 7)) { if (!first) strcat(out, ", "); strcat(out, "opc"); first = 0; }
    if (mask9 & (1 << 6)) { if (!first) strcat(out, ", "); strcat(out, "tos"); first = 0; }
    if (mask9 & (1 << 5)) { if (!first) strcat(out, ", "); strcat(out, "cpp"); first = 0; }
    if (mask9 & (1 << 4)) { if (!first) strcat(out, ", "); strcat(out, "lv"); first = 0; }
    if (mask9 & (1 << 3)) { if (!first) strcat(out, ", "); strcat(out, "sp"); first = 0; }
    if (mask9 & (1 << 2)) { if (!first) strcat(out, ", "); strcat(out, "pc"); first = 0; }
    if (mask9 & (1 << 1)) { if (!first) strcat(out, ", "); strcat(out, "mdr"); first = 0; }
    if (mask9 & (1 << 0)) { if (!first) strcat(out, ", "); strcat(out, "mar"); first = 0; }
    if (out[0] == '\0') strcpy(out, "none");
}

/* -------------------- Dump de registradores no formato binário pedido ----- */
void dump_regs_bin(FILE *f) {
    char b[33], b8[9];
    to_bin32(regs.MAR, b); fprintf(f, "mar = %s\n", b);
    to_bin32(regs.MDR, b); fprintf(f, "mdr = %s\n", b);
    to_bin32(regs.PC,  b); fprintf(f, "pc = %s\n", b);
    to_bin8(regs.MBR, b8); fprintf(f, "mbr = %s\n", b8);
    to_bin32(regs.SP,  b); fprintf(f, "sp = %s\n", b);
    to_bin32(regs.LV,  b); fprintf(f, "lv = %s\n", b);
    to_bin32(regs.CPP, b); fprintf(f, "cpp = %s\n", b);
    to_bin32(regs.TOS, b); fprintf(f, "tos = %s\n", b);
    to_bin32(regs.OPC, b); fprintf(f, "opc = %s\n", b);
    to_bin32(regs.H,   b); fprintf(f, "h = %s\n", b);
}

/* -------------------- Ler arquivo de registradores no formato textual ----- */
int load_regs_from_file(const char *fname) {
    FILE *f = fopen(fname, "r");
    if (!f) return 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p && isspace((unsigned char)*p)) ++p;
        if (*p == '\0') continue;
        // form: name = bits
        char name[32];
        if (sscanf(p, "%31s", name) != 1) continue;
        // remove trailing '=' or spaces from name if present
        char *eq = strchr(p, '=');
        if (!eq) continue;
        ++eq;
        while (*eq && isspace((unsigned char)*eq)) ++eq;
        // eq now points to the binary string
        if (strncmp(name, "mar", 3) == 0) regs.MAR = parse_bin32(eq);
        else if (strncmp(name, "mdr", 3) == 0) regs.MDR = parse_bin32(eq);
        else if (strncmp(name, "pc", 2) == 0)  regs.PC  = parse_bin32(eq);
        else if (strncmp(name, "mbr", 3) == 0) regs.MBR = parse_bin8(eq);
        else if (strncmp(name, "sp", 2) == 0)  regs.SP  = parse_bin32(eq);
        else if (strncmp(name, "lv", 2) == 0)  regs.LV  = parse_bin32(eq);
        else if (strncmp(name, "cpp", 3) == 0) regs.CPP = parse_bin32(eq);
        else if (strncmp(name, "tos", 3) == 0) regs.TOS = parse_bin32(eq);
        else if (strncmp(name, "opc", 3) == 0) regs.OPC = parse_bin32(eq);
        else if (strncmp(name, "h", 1) == 0)   regs.H   = parse_bin32(eq);
    }
    fclose(f);
    return 1;
}

/* -------------------- Trim auxiliar -------------------- */
void trim_trail(char *s) {
    int l = (int)strlen(s);
    while (l > 0 && isspace((unsigned char)s[l-1])) s[--l] = '\0';
}

/* -------------------- MAIN -------------------- */
int main(void) {
    const char *regs_file = "registradores_etapa2_tarefa2.txt";
    const char *prog_file = "programa_etapa2_tarefa2.txt";
    const char *out_file  = "saida_etapa2_tarefa2.txt";

    if (!load_regs_from_file(regs_file)) {
        fprintf(stderr, "Erro: nao foi possivel abrir %s\n", regs_file);
        return 1;
    }

    FILE *fin = fopen(prog_file, "r");
    FILE *fout = fopen(out_file, "w");
    if (!fout) {
        fprintf(stderr, "Erro criando %s\n", out_file);
        if (fin) fclose(fin);
        return 1;
    }

    /* Imprime estado inicial conforme seu exemplo */
    fprintf(fout, "=====================================================\n");
    fprintf(fout, "> Initial register states\n");
    dump_regs_bin(fout);
    fprintf(fout, "\n=====================================================\n");
    fprintf(fout, "Start of program\n");
    fprintf(fout, "=====================================================\n");

    /* Se não existe arquivo de programa, imprime EOP imediatamente */
    if (!fin) {
        fprintf(fout, "Cycle 1\n");
        fprintf(fout, "No more lines, EOP.\n");
        fprintf(fout, "=====================================================\n");
        fclose(fout);
        return 0;
    }

    char line[256];
    int cycle = 0;
    while (1) {
        /* lê próxima linha sem descartar ciclo ? queremos igual comportamento do exemplo:
           ler linha, se eof -> incrementar ciclo e print EOP. */
        if (!fgets(line, sizeof(line), fin)) {
            /* EOF */
            cycle++;
            fprintf(fout, "Cycle %d\n", cycle);
            fprintf(fout, "No more lines, EOP.\n");
            fprintf(fout, "=====================================================\n");
            break;
        }

        trim_trail(line);
        if (strlen(line) == 0) continue;

        cycle++;
        /* IR: 21 bits. Queremos imprimir separado: 8bits 9bits 4bits */
        char ir8[9], ir9[10], ir4[5];
        memset(ir8, 0, sizeof(ir8)); memset(ir9, 0, sizeof(ir9)); memset(ir4, 0, sizeof(ir4));
        // assumir que line tem pelo menos 21 caracteres 0/1; se tiver espaços, ignorar primeiro 21 índices de 0/1
        char bits[22]; int bi = 0;
        for (int i = 0; line[i] && bi < 21; ++i) if (line[i] == '0' || line[i] == '1') bits[bi++] = line[i];
        bits[bi] = '\0';
        if (bi < 21) {
            // linha inválida, tratar como terminadora
            fprintf(fout, "Cycle %d\n", cycle);
            fprintf(fout, "> Line invalid or too short, EOP.\n");
            fprintf(fout, "=====================================================\n");
            break;
        }
        strncpy(ir8, bits, 8); ir8[8] = '\0';
        strncpy(ir9, bits + 8, 9); ir9[9] = '\0';
        strncpy(ir4, bits + 17, 4); ir4[4] = '\0';

        fprintf(fout, "Cycle %d\n", cycle);
        fprintf(fout, "ir = %s %s %s\n\n", ir8, ir9, ir4);

        /* decodifica controles */
        // converter as substrings para inteiros
        int controlULA = 0;
        for (int i = 0; i < 8; ++i) controlULA = (controlULA << 1) | (ir8[i] - '0');
        int controlC = 0;
        for (int i = 0; i < 9; ++i) controlC = (controlC << 1) | (ir9[i] - '0');
        int controlB = 0;
        for (int i = 0; i < 4; ++i) controlB = (controlB << 1) | (ir4[i] - '0');

        /* b_bus name + value */
        char bname[16];
        uint32_t Bval = get_B_value_and_name(controlB, bname);
        fprintf(fout, "b_bus = %s\n", bname);

        /* c_bus names */
        char cnames[256];
        c_bus_names(controlC, cnames);
        fprintf(fout, "c_bus = %s\n\n", cnames);

        /* Registers before */
        fprintf(fout, "> Registers before instruction\n");
        dump_regs_bin(fout);
        fprintf(fout, "\n");

        /* Executa ULA: A = H, B = Bval */
        uint32_t A = regs.H;
        ULAOut u = ula_exec(A, Bval, (unsigned char)controlULA);
        if (u.error) {
            fprintf(fout, "> Error: invalid control signals in ULA.\n\n");
            fprintf(fout, "> Registers after instruction\n");
            dump_regs_bin(fout);
            fprintf(fout, "=====================================================\n");
            continue;
        }

        /* Escrever Sd nos registradores habilitados por controlC */
        write_C_regs(controlC, u.Sd);

        /* Registros depois */
        fprintf(fout, "> Registers after instruction\n");
        dump_regs_bin(fout);
        fprintf(fout, "=====================================================\n");
    }

    fclose(fin);
    fclose(fout);
    return 0;
}
