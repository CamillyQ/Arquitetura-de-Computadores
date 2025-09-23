#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Imprimir inteiro de 32 bits em binário
void print_bin32(FILE *out, int32_t val) {
    for (int i = 31; i >= 0; i--) {
        fprintf(out, "%d", (val >> i) & 1);
    }
}

// Função da ULA
void ula(int32_t A, int32_t B, int instrucao[6], int32_t *S, int *carry) {
    int F0 = instrucao[0];
    int F1 = instrucao[1];
    int ENA = instrucao[2];
    int ENB = instrucao[3];
    int INVA = instrucao[4];
    int INC  = instrucao[5];

    // Habilita entradas
    int32_t a_val = ENA ? A : 0;
    int32_t b_val = ENB ? B : 0;

    // Inverte A se necessário
    if (INVA) {
        a_val = ~a_val;
    }

    // Operação base
    *carry = 0;
    if (F0 == 0 && F1 == 0) {
        *S = a_val & b_val;          // AND
    } else if (F0 == 0 && F1 == 1) {
        *S = a_val | b_val;          // OR
    } else if (F0 == 1 && F1 == 0) {
        *S = a_val ^ b_val;          // XOR
    } else if (F0 == 1 && F1 == 1) {
        int64_t tmp = (int64_t)a_val + (int64_t)b_val;
        *S = (int32_t)tmp;
        if (tmp > 0x7FFFFFFF || tmp < (int64_t)0xFFFFFFFF80000000) {
            *carry = 1;
        }
    }

    // Incremento (vem-um forçado)
    if (INC) {
        int64_t tmp = (int64_t)(*S) + 1;
        *S = (int32_t)tmp;
        if (tmp > 0x7FFFFFFF || tmp < (int64_t)0xFFFFFFFF80000000) {
            *carry = 1;
        }
    }
}

int main() {
    FILE *entrada = fopen("programa_etapa1.txt", "r");
    FILE *saida   = fopen("saida_etapa1.txt", "w");

    if (!entrada || !saida) {
        printf("Erro ao abrir arquivos!\n");
        return 1;
    }

    char linha[20];
    int PC = 1; // começa em 1 como no exemplo

    // Valores fixos
    int32_t A = -1; // 11111111111111111111111111111111
    int32_t B = 1;  // 00000000000000000000000000000001

    fprintf(saida, "b = "); print_bin32(saida, B); fprintf(saida, "\n");
    fprintf(saida, "a = "); print_bin32(saida, A); fprintf(saida, "\n\n");

    fprintf(saida, "Start of Program\n");
    fprintf(saida, "============================================================\n");

    int ciclo = 1;
    while (fgets(linha, sizeof(linha), entrada)) {
        if (strlen(linha) < 6) break; // ignora linha vazia

        int instrucao[6];
        for (int i = 0; i < 6; i++) {
            instrucao[i] = linha[i] - '0';
        }

        int32_t S;
        int carry;
        ula(A, B, instrucao, &S, &carry);

        fprintf(saida, "Cycle %d\n\n", ciclo);
        fprintf(saida, "PC = %d\n", PC);
        fprintf(saida, "IR = ");
        for (int i = 0; i < 6; i++) fprintf(saida, "%d", instrucao[i]);
        fprintf(saida, "\n");

        fprintf(saida, "b = "); print_bin32(saida, B); fprintf(saida, "\n");
        fprintf(saida, "a = "); print_bin32(saida, A); fprintf(saida, "\n");
        fprintf(saida, "s = "); print_bin32(saida, S); fprintf(saida, "\n");
        fprintf(saida, "co = %d\n", carry);
        fprintf(saida, "============================================================\n");

        PC++;
        ciclo++;
    }

    fprintf(saida, "Cycle %d\n\n", ciclo);
    fprintf(saida, "PC = %d\n", PC);
    fprintf(saida, "> Line is empty, EOP.\n");

    fclose(entrada);
    fclose(saida);

    printf("Execução concluída. Veja saida_etapa1.txt\n");
    return 0;
}
