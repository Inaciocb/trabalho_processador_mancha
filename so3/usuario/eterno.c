// eterno: nunca termina; o init o mata no fim

#include "lib_usuario.c"

int main(void)
{
    int x;
    x = 0;
    for (;;) {
        x = x + 1;
    }
    return 0;
}
