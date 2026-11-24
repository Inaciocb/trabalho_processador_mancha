// invasor.c -- tenta usar a console sem passar pelo SO: "out" em modo
// usuário causa a interrupção 2 (instrução privilegiada) e o SO o mata.

#include "so.h"

int main(void)
{
    u_escreve("invasor: tentando E/S direta\n");
    tenta_es();
    u_escreve("invasor: isto nao deveria aparecer\n");
    return 0;
}
