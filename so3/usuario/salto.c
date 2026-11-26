// salto: salta para uma página sem código

#include "lib_usuario.c"

int main(void)
{
    void (*f)(void);

    u_escreve("salto: saltando para 0x7000\n");
    f = 0x7000;
    f();
    u_escreve("salto: isto nao deveria aparecer\n");
    return 0;
}
