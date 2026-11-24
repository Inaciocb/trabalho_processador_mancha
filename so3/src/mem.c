// mem.c -- gerenciador de quadros físicos livres

#include "mem.h"

void k_puts(char *s);
void k_int(int v);
void para(void);

struct quadro quadros[MAX_QUADROS];
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
        quadros[q].livre = q >= primeiro && q <= ultimo;
        if (quadros[q].livre) {
            n_livres = n_livres + 1;
        }
    }
}

int aloca_quadro(void)
{
    int q;

    for (q = q_primeiro; q <= q_ultimo; q++) {
        if (quadros[q].livre) {
            quadros[q].livre = 0;
            n_livres = n_livres - 1;
            return q;
        }
    }
    return -1;
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

// comparações sempre por número de quadro: q * 512 passa de 32767 em 16 bits
int *end_quadro(int q)
{
    return (int *) (q * TAM_QUADRO);
}
