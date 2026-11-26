// pilha: recursão sem fim, até a pilha sair da sua página

#include "lib_usuario.c"

int desce(int n)
{
    if (n % 16 == 0) {
        u_escreve_int("pilha: nivel ", n, "\n");
    }
    return desce(n + 1) + 1;
}

int main(void)
{
    return desce(1);
}
