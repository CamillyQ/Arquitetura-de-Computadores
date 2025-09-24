#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

typedef struct {
    uint32_t S;     // saída antes do deslocamento
    uint32_t Sd;    // saída final
    int carry;      // vai-um
    int N;          // negativo (bit 31)
    int Z;          // zero
    int error;      // erro de sinais de controle
} ULAOut;

void to_binary32(uint32_t x, char *out) {
    for (int i = 31; i >= 0; --i)
        out[31 - i] = (x >> i) & 1 ? '1' : '0';
    out[32] = '\0';
}

static int parse_binary8(const char *s) {
    int v = 0;
    for (int i = 0; s[i] && i < 8; ++i) {
        if (s[i] == '0' || s[i] == '1')
            v = (v << 1) | (s[i] - '0');
    }
    return v;
}

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
        out.carry = (sum >> 32) & 1u;
    } else {
        switch (op) {
            case 0: out.S = a & b; break;
            case 1: out.S = a | b; break;
            case 3: out.S = a ^ b; break;
        }
        if (INC) out.S = out.S + 1u;
        out.carry = 0;
    }

    out.Sd = out.S;
    if (SLL8) {
        out.Sd = (out.S << 8) & 0xFFFFFFFFu;
    } else if (SRA1) {
        int32_t tmp = (int32_t)out.S;
        out.Sd = (uint32_t)(tmp >> 1);
    }

    // Calcula N e Z a partir do Sd (resultado final)
    out.N = 0;
    out.Z = (out.Sd == 0);

    return out;
}

int main() {
    FILE *fin = fopen("programa_etapa2_tarefa1.txt", "r");
    FILE *fout = fopen("saida_etapa2_tarefa1.txt", "w");
    if (!fin || !fout) {
        fprintf(stderr, "Erro abrindo arquivos.\n");
        return 1;
    }

    uint32_t A = 0x00000001;
    uint32_t B = 0x80000000;

    int N = 0, Z = 0, CO = 0; // Flags persistentes entre ciclos

    char linha[256];
    int PC = 0;

    fprintf(fout, "Start of Program\n============================================================\n");

    while (1) {
        PC++; // PC começa em 1
        fprintf(fout, "Cycle %d\n\n", PC);

        if (!fgets(linha, sizeof(linha), fin)) {
            fprintf(fout, "PC = %d\n", PC);
            fprintf(fout, "> Line is empty, EOP.\n");
            fprintf(fout, "============================================================\n");
            break;
        }

        int len = strlen(linha);
        while (len > 0 && isspace((unsigned char)linha[len-1])) linha[--len] = '\0';

        if (len == 0) {
            fprintf(fout, "PC = %d\n", PC);
            fprintf(fout, "> Line is empty, EOP.\n");
            fprintf(fout, "============================================================\n");
            continue;
        }

        int control = parse_binary8(linha);
        ULAOut out = ula_exec(A, B, (unsigned char)control);

        fprintf(fout, "PC = %d\n", PC);
        fprintf(fout, "IR = %s\n", linha);

        if (out.error) {
            fprintf(fout, "> Error, invalid control signals.\n");
        } else {
            char binA[33], binB[33], binS[33], binSd[33];
            to_binary32(A, binA);
            to_binary32(B, binB);
            to_binary32(out.S, binS);
            to_binary32(out.Sd, binSd);

            // --- Atualização condicional dos flags ---
            int F0   = (control >> 5) & 1;
            int F1   = (control >> 4) & 1;
            int op   = (F0 << 1) | F1;

            // atualiza flags apenas para operações lógicas/aritméticas
            if (op == 0 || op == 1 || op == 2 || op == 3) {
                N = out.N;
                Z = out.Z;
                CO = out.carry;
            }

            fprintf(fout, "b = %s\n", binB);
            fprintf(fout, "a = %s\n", binA);
            fprintf(fout, "s = %s\n", binS);
            fprintf(fout, "sd = %s\n", binSd);
            fprintf(fout, "n = %d\n", N);
            fprintf(fout, "z = %d\n", Z);
            fprintf(fout, "co = %d\n", CO);
        }

        fprintf(fout, "============================================================\n");
    }

    fclose(fin);
    fclose(fout);
    return 0;
}
