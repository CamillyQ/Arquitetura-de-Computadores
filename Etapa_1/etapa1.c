#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int F0, F1, ENA, ENB, INVA, INC;
} Instrucao;

Instrucao parse_instr(const char *line) {
    Instrucao inst;
    inst.F0   = line[0] - '0';
    inst.F1   = line[1] - '0';
    inst.ENA  = line[2] - '0';
    inst.ENB  = line[3] - '0';
    inst.INVA = line[4] - '0';
    inst.INC  = line[5] - '0';
    return inst;
}

void full_adder(int a, int b, int cin, int *s, int *cout) {
    *s = (a ^ b) ^ cin;
    *cout = (a & b) | (cin & (a ^ b));
}

void execute_instr(Instrucao inst, int A, int B, int vem_um, int *S, int *vai_um) {
    int a_in = inst.ENA ? A : 0;
    if (inst.INVA) a_in = 1 - a_in;
    int b_in = inst.ENB ? B : 0;
    int cin = (vem_um || inst.INC) ? 1 : 0;

    int f = (inst.F0 << 1) | inst.F1;

    switch (f) {
        case 0b00:
            *S = a_in & b_in;
            *vai_um = 0;
            break;
        case 0b01:
            *S = a_in | b_in;
            *vai_um = 0;
            break;
        case 0b10: 
            full_adder(a_in, b_in, cin, S, vai_um);
            break;
        case 0b11:
            full_adder(a_in, b_in, cin, S, vai_um);
            break;
        default:
            *S = 0;
            *vai_um = 0;
    }
}

int main() {

    char input_file[] = "programa_etapa1.txt";
    char output_file[] = "saida_etapa1.txt";


    int A = 1, B = 1;
    int PC = 0;
    int vem_um = 0;


    FILE *fin = fopen(input_file, "r");
    if (!fin) {
        printf("Erro: nao consegui abrir o arquivo de entrada '%s'.\n", input_file);
        printf("Certifique-se de que ele esta na mesma pasta do executavel.\n");
        return 1;
    }

    FILE *fout = fopen(output_file, "w");
    if (!fout) {
        printf("Erro: nao consegui criar o arquivo de saida '%s'.\n", output_file);
        fclose(fin);
        return 1;
    }

    char line[64];
    while (fgets(line, sizeof(line), fin)) {
        if (strlen(line) < 6) continue;
        line[strcspn(line, "\r\n")] = 0; 

        Instrucao inst = parse_instr(line);
        int S, vai_um;
        execute_instr(inst, A, B, vem_um, &S, &vai_um);

        fprintf(fout, "IR=%s PC=%d A=%d B=%d S=%d Vai-um=%d\n",
                line, PC, A, B, S, vai_um);

        vem_um = vai_um;
        PC++;
    }

    fclose(fin);
    fclose(fout);

    printf("Simulacao concluida.\nArquivo de saida criado: %s\n", output_file);
    return 0;
}
