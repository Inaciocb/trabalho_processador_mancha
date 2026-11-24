// misto: alterna um pouco de cálculo com escrita

#include "lib_usuario.c"

#define MISTO_PASSOS  6
#define MISTO_CONTA   1500

int main(void)
{
    int passo;
    int i;
    int x;

    x = 0;
    for (passo = 1; passo <= MISTO_PASSOS; passo++) {
        for (i = 0; i < MISTO_CONTA; i++) {
            x = x + i;
        }
        u_escreve_int("misto: passo ", passo, "\n");
    }
    return 0;
}
