// salta_longe.c -- teste de proteção: chama o endereço 0x6000 (página 48),
// além do código do programa. A busca de instrução falta (interrupção 5)
// com ip = 0x6000, página >= tamanho do código: o SO mata o processo.

#include "so.h"

int main(void)
{
    void (*longe)(void);

    u_escreve("salta_longe: chamando o endereco 0x6000\n");
    longe = 0x6000;
    longe();
    u_escreve("salta_longe: isto nao deveria aparecer\n");
    return 0;
}
