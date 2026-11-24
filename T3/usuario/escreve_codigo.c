// escreve_codigo.c -- teste de proteção: escreve na página 0, que é de
// código. A entrada de dados da página 0 é um espelho SÓ LEITURA (tipo 1)
// do quadro de código, então a escrita em modo usuário causa a
// interrupção 1 e o SO mata o processo.

#include "so.h"

int main(void)
{
    u_escreve("escreve_codigo: escrevendo no endereco 0x0010 (codigo)\n");
    *(int *) 0x0010 = 1;
    u_escreve("escreve_codigo: isto nao deveria aparecer\n");
    return 0;
}
