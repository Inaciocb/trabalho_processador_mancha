// le_longe.c -- teste de proteção: lê um endereço (0x7000, página 56) que
// não pertence ao processo. A entrada de dados está ausente (tipo 0):
// interrupção 5, falta de DADO, e o SO mata o processo.

#include "so.h"

int x;

int main(void)
{
    u_escreve("le_longe: lendo o endereco 0x7000\n");
    x = *(int *) 0x7000;
    u_escreve("le_longe: isto nao deveria aparecer\n");
    return 0;
}
