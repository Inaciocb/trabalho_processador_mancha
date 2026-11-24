// mem.c -- gerenciador de quadros físicos livres

#include "mem.h"

void k_puts(char *s);
void k_int(int v);
void para(void);

struct quadro quadros[MAX_QUADROS];
int livres[MAX_QUADROS];      // pilha dos quadros livres
int n_livres;
int q_primeiro;
int q_ultimo;

void mem_inicia(int primeiro, int ultimo)
{
    int q;

    q_primeiro = primeiro;
    q_ultimo = ultimo;
    n_livres = 0;
    for (q = 0; q < MAX_QUADROS; q++) {
        quadros[q].livre = 0;
    }
    for (q = ultimo; q >= primeiro; q--) {
        quadros[q].livre = 1;
        livres[n_livres] = q;
        n_livres = n_livres + 1;
    }
}

int aloca_quadro(void)
{
    int q;

    if (n_livres == 0) {
        return -1;
    }
    n_livres = n_livres - 1;
    q = livres[n_livres];
    quadros[q].livre = 0;
    return q;
}

void libera_quadro(int q)
{
    if (q < q_primeiro || q > q_ultimo || quadros[q].livre) {
        k_puts("\n[SO] PANICO: libera_quadro(");
        k_int(q);
        k_puts(") invalido\n");
        para();
        return;
    }
    quadros[q].livre = 1;
    livres[n_livres] = q;
    n_livres = n_livres + 1;
}

int quadros_livres(void)
{
    return n_livres;
}

int mem_primeiro(void)
{
    return q_primeiro;
}

int mem_ultimo(void)
{
    return q_ultimo;
}

#ifdef MEM_FALSA
int memoria_falsa[MAX_QUADROS * TAM_QUADRO / 2];

int *end_quadro(int q)
{
    return &memoria_falsa[q * TAM_QUADRO / 2];
}
#else
// comparações sempre por número de quadro: q * 512 passa de 32767 em 16 bits
int *end_quadro(int q)
{
    return (int *) (q * TAM_QUADRO);
}
#endif
