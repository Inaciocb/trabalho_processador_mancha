// gemeo: roda em duas cópias ao mesmo tempo, cada uma contando na sua
// global; com memória compartilhada o contador pularia números

#include "lib_usuario.c"

#define GEMEO_PASSOS  5
#define GEMEO_CONTA   3000

int contador;

int main(void)
{
    int passo;
    int i;
    int x;

    x = 0;
    for (passo = 1; passo <= GEMEO_PASSOS; passo++) {
        for (i = 0; i < GEMEO_CONTA; i++) {
            x = x + i;
        }
        contador = contador + 1;
        u_escreve_int("gemeo: contador=", contador, "\n");
    }
    return 0;
}
