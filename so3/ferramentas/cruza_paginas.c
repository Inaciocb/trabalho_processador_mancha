// cruza_paginas.c -- marca as páginas de código que começam no meio de uma
// instrução (a segunda palavra de uma instrução de 4 bytes).
//
// uso: cruza_paginas prog.bin
//
// Imprime uma marca (0 ou 1) por página de código. O código vai do
// endereço 8 (depois da capa, ver usuario/entrada.asm) até o início dos
// dados, e só tem instruções: decodificar em sequência é exato.

#include <stdio.h>
#include "instrucao.h"

#define TAM_PAGINA 512

int main(int argc, char *argv[])
{
    static unsigned char img[65536];
    FILE *f;
    size_t n;
    int ini, pc, p;
    static int cruza[128];

    if (argc != 2) {
        fprintf(stderr, "uso: %s prog.bin\n", argv[0]);
        return 1;
    }
    f = fopen(argv[1], "rb");
    if (f == NULL) {
        perror(argv[1]);
        return 1;
    }
    n = fread(img, 1, sizeof(img), f);
    fclose(f);
    if (n < 8) {
        fprintf(stderr, "%s: imagem curta\n", argv[1]);
        return 1;
    }
    ini = img[6] * 256 + img[7];
    if (ini < 8 || (size_t)ini > n || ini % TAM_PAGINA != 0) {
        fprintf(stderr, "%s: inicio dos dados invalido (%d)\n", argv[1], ini);
        return 1;
    }

    pc = 8;
    while (pc < ini) {
        instr_decod_t d;
        int tam;

        instrucao_decodifica((uint16_t)(img[pc] * 256 + img[pc + 1]), &d);
        tam = (d.info != NULL && instrucao_requer_imediato(&d)) ? 4 : 2;
        if (tam == 4 && (pc + 2) % TAM_PAGINA == 0) {
            cruza[(pc + 2) / TAM_PAGINA] = 1;
        }
        pc += tam;
    }
    if (pc != ini) {
        fprintf(stderr, "%s: ultima instrucao invade os dados\n", argv[1]);
        return 1;
    }

    for (p = 0; p < ini / TAM_PAGINA; p++) {
        printf("%s%d", p ? " " : "", cruza[p]);
    }
    printf("\n");
    return 0;
}
