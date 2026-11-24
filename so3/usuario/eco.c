// eco: I/O-bound, lê linhas da console (bloqueia esperando o operador) e
// as devolve em maiúsculas

#include "lib_usuario.c"

#define ECO_LINHAS    3

int main(void)
{
    char linha[40];
    int n;
    int c;
    int lidas;

    u_escreve("eco: esperando 3 linhas\n");
    lidas = 0;
    n = 0;
    while (lidas < ECO_LINHAS) {
        c = getchar();
        if (c == '\n') {
            linha[n] = 0;
            u_escreve("eco: ");
            u_escreve(linha);
            putchar('\n');
            n = 0;
            lidas = lidas + 1;
        } else if (n < 39) {
            if (c >= 'a' && c <= 'z') {
                c = c - 32;
            }
            linha[n] = c;
            n = n + 1;
        }
    }
    return 0;
}
