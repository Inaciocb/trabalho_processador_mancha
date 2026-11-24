// lib_usuario.c -- funções comuns aos programas de usuário. Cada programa
// é compilado sozinho (o mcc compila um arquivo por vez e não há ligador),
// então quem usa estas funções as inclui com #include "lib_usuario.c".

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
