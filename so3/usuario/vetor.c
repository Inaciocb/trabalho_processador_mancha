// vetor: tenta escrever no endereço 0, onde fica o vetor de interrupções
// na memória física

#include "lib_usuario.c"

int main(void)
{
    int *p;

    u_escreve("vetor: escrevendo no endereco 0\n");
    p = (int *) 0;
    *p = 1;
    u_escreve("vetor: isto nao deveria aparecer\n");
    return 0;
}
