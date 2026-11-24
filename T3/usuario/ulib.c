// ulib.c -- funções auxiliares comuns aos programas de usuário. Compilada
// uma vez e montada junto com cada programa (não há ligador: cada
// programa é um módulo só, montado a partir do endereço virtual 0).

#include <mancha.h>
#include "so.h"

void u_escreve(char *s)
{
    while (*s != 0) {
        putchar(*s);
        s = s + 1;
    }
}

void u_escreve_int(char *antes, int v, char *depois)
{
    u_escreve(antes);
    print_int(v);
    u_escreve(depois);
}
