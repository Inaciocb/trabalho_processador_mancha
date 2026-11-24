// mem.h -- gerenciador de quadros físicos livres

#ifndef MEM_H
#define MEM_H

#define TAM_QUADRO   512
#define MAX_QUADROS  128      // só os primeiros 64 KB são alcançáveis (ver README)
#define QUADRO_LIMITE 104     // 0xD000: acima ficam as pilhas da BIOS

struct quadro {
    int livre;
};

void mem_inicia(int primeiro, int ultimo);
int  aloca_quadro(void);
void libera_quadro(int q);
int  quadros_livres(void);
int  mem_primeiro(void);
int  mem_ultimo(void);
int *end_quadro(int q);

#endif
