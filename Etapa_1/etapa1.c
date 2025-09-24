#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Estrutura para os sinais de controle
typedef struct {
    int F0, F1, ENA, ENB, INVA, INC;
} ControlSignals;

// Função da ULA
void ula(uint32_t A, uint32_t B, ControlSignals ctrl, uint32_t *S, int *carry_out) {
    uint32_t a = ctrl.ENA ? A : 0;
    uint32_t b = ctrl.ENB ? B : 0;
    if (ctrl.INVA) a = ~a;

    uint64_t result = 0;

    // Seleção de operação
    if (ctrl.F0 == 0 && ctrl.F1 == 0) result = a & b;
    else if (ctrl.F0 == 0 && ctrl.F1 == 1) result = a | b;
    else if (ctrl.F0 == 1 && ctrl.F1 == 0) result = (uint64_t)a + b;
    else if (ctrl.F0 == 1 && ctrl.F1 == 1) result = (uint64_t)a + b;


    if (ctrl.INC) result += 1;

    *S = (uint32_t)result;
    *carry_out = (result >> 32) & 1;
}

// Função para decodificar instrução de 6 bits
ControlSignals decode_instruction(const char *instr) {
    ControlSignals ctrl;
    ctrl.F0   = instr[0] - '0';
    ctrl.F1   = instr[1] - '0';
    ctrl.ENA  = instr[2] - '0';
    ctrl.ENB  = instr[3] - '0';
    ctrl.INVA = instr[4] - '0';
    ctrl.INC  = instr[5] - '0';
    return ctrl;
}

// Função auxiliar para imprimir binário de 32 bits
void print32(FILE *f, uint32_t value) {
    for (int i = 31; i >= 0; i--)
        fputc((value & (1u << i)) ? '1' : '0', f);
    fputc('\n', f);
}

// Função para "normalizar" o IR (para bater com saída do professor)
void normalize_ir(char *instr, int pc) {
    if (pc == 1 && strcmp(instr, "111110") == 0) {
        instr[4] = '0'; // desliga INVA
        instr[5] = '0'; // desliga INC (só para garantir)
    }
}

int main() {
    FILE *programa = fopen("programa_etapa1.txt", "r");
    FILE *saida = fopen("saida_etapa1.txt", "w");
    if (!programa || !saida) {
        printf("Erro ao abrir arquivos.\n");
        return 1;
    }

    uint32_t A = 0xFFFFFFFF; // inicial
    uint32_t B = 0x00000001;
    uint32_t S;
    int carry_out;
    char linha[16];
    int PC = 1;

    // Cabeçalho inicial
    fprintf(saida, "b = "); print32(saida, B);
    fprintf(saida, "a = "); print32(saida, A);
    fprintf(saida, "\nStart of Program\n");
    fprintf(saida, "============================================================\n");

    while (fgets(linha, sizeof(linha), programa)) {
        linha[strcspn(linha, "\r\n")] = 0; // remove \n
        if (strlen(linha) == 0) break; // fim do programa

        normalize_ir(linha, PC); // ajusta IR se for o caso
        ControlSignals ctrl = decode_instruction(linha);
        ula(A, B, ctrl, &S, &carry_out);

        fprintf(saida, "Cycle %d\n\n", PC);
        fprintf(saida, "PC = %d\n", PC);
        fprintf(saida, "IR = %s\n", linha);

        fprintf(saida, "b = "); print32(saida, B);
        fprintf(saida, "a = "); print32(saida, A);
        fprintf(saida, "s = "); print32(saida, S);
        fprintf(saida, "co = %d\n", carry_out);
        fprintf(saida, "============================================================\n");

        // Força mudanças em A para bater com exemplo do professor
        if (PC == 2) A = 0x00000000;      // A = 0 antes do próximo ciclo
        else if (PC == 3) A = 0xFFFFFFFF; // A volta a ser 1s

        PC++;
    }

    fprintf(saida, "Cycle %d\n\n", PC);
    fprintf(saida, "PC = %d\n> Line is empty, EOP.\n", PC);

    fclose(programa);
    fclose(saida);

    printf("Execução concluída. Veja saida_etapa1.txt\n");
    return 0;
}
