// invasor: tenta usar a console sem passar pelo SO

#include "lib_usuario.c"

int main(void)
{
    u_escreve("invasor: tentando E/S direta\n");
    tenta_es();
    u_escreve("invasor: isto nao deveria aparecer\n");
    return 0;
}
