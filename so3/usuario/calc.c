// calc: CPU-bound, conta primos por divisão sem nenhuma chamada no meio

#include "lib_usuario.c"

#define CALC_LIMITE   900

int main(void)
{
    int n;
    int d;
    int primo;
    int total;

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
    u_escreve_int("calc: ", total, " primos\n");
    return 0;
}
