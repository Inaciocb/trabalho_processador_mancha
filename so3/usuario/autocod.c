// autocod: tenta escrever por cima do próprio código

#include "lib_usuario.c"

int main(void)
{
    int (*f)(void);
    int *p;

    f = main;
    p = (int *) f;
    u_escreve_int("autocod: escrevendo no codigo, endereco ", (int) p, "\n");
    *p = 0;
    u_escreve("autocod: isto nao deveria aparecer\n");
    return 0;
}
