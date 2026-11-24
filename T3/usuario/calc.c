// calc.c -- CPU-bound (como no T2): conta primos por divisão, sem chamadas
// no meio. O contador é uma variável GLOBAL: rodando o mesmo binário em
// dois processos, cada um imprime o mesmo endereço virtual e o mesmo
// valor -- cada processo tem o seu quadro de dados por trás desse endereço.

#include <mancha.h>
#include "so.h"

#define CALC_LIMITE 900

int total;

int main(void)
{
    int n;
    int d;
    int primo;

    u_escreve("calc: inicio\n");
    total = 0;
    for (n = 2; n < CALC_LIMITE; n++) {
        primo = 1;
        d = 2;
        while (d * d <= n) {
            if (n % d == 0) {
                primo = 0;
                break;
            }
            d = d + 1;
        }
        if (primo) {
            total = total + 1;
        }
    }
    u_escreve_int("calc: ", total, " primos, contador no endereco virtual ");
    print_hex((int) &total);
    putchar('\n');
    return 0;
}
