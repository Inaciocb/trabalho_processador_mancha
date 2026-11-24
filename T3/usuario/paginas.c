// paginas.c -- programa com código grande (várias páginas) e localidade
// controlada, para comparar FIFO e LRU na substituição de páginas.
//
// O mcc emite as funções na ordem do fonte, então main e quente ficam nas
// primeiras páginas e cada função "fria" ocupa sozinha mais de uma página
// (o mapa função -> página sai no relatório do mkdisco, build/mkdisco.txt).
// O laço chama quente() sempre e, de vez em quando, uma das frias, em
// rodízio. Com poucos quadros, o LRU mantém as páginas de main/quente
// (acessadas a todo tique) e expulsa as frias; o FIFO expulsa pela ordem
// de carga, inclusive as páginas quentes, que voltam a faltar logo depois.

#include <mancha.h>
#include "so.h"

#define ITERACOES  240
#define PERIODO    6     // uma fria a cada PERIODO iterações
#define N_FRIAS    4

// passo de cálculo sem sentido, só para ocupar espaço de código
#define L1  x = (x * 3 + 7) % 1000;
#define L4  L1 L1 L1 L1
#define L12 L4 L4 L4

int quente(int x);
int fria1(int x);
int fria2(int x);
int fria3(int x);
int fria4(int x);

int main(void)
{
    int i;
    int x;
    int k;

    u_escreve("paginas: inicio\n");
    x = 1;
    k = 0;
    for (i = 0; i < ITERACOES; i++) {
        x = quente(x);
        if (i % PERIODO == 0) {
            if (k == 0) x = fria1(x);
            else if (k == 1) x = fria2(x);
            else if (k == 2) x = fria3(x);
            else x = fria4(x);
            k = (k + 1) % N_FRIAS;
        }
    }
    u_escreve_int("paginas: resultado ", x, "\n");
    return 0;
}

int quente(int x)
{
    int j;
    for (j = 0; j < 30; j++) {
        x = (x * 7 + j) % 1000;
    }
    return x;
}

int fria1(int x) { L12 return x; }
int fria2(int x) { L12 return x; }
int fria3(int x) { L12 return x; }
int fria4(int x) { L12 return x; }
