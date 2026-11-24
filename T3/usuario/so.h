// so.h -- interface das chamadas de sistema para programas de usuário
// (implementada em runtime_so.asm) e das funções auxiliares comuns
// (ulib.c). As funções de mancha.h (putchar, getchar, puts, print_int,
// print_hex) também funcionam em modo usuário: o runtime_so.asm
// reimplementa mancha_out/mancha_in/mancha_halt por cima destas chamadas.
//
// Cada programa é montado sozinho, a partir do endereço virtual 0, junto
// com crt0.asm (entrada: chama main e depois SO_MATA_PROC(0)), ulib.c,
// runtime_so.asm e a biblioteca do mcc.

#ifndef SO_H
#define SO_H

#include "so_chamadas.h"

int so_le(void);
int so_escreve(int c);
int so_cria_proc(int prog);
int so_mata_proc(int pid);
int so_espera_proc(int pid);

// tenta escrever direto na porta da console: em modo usuário (I=0) causa a
// interrupção 2, e o SO mata o processo
void tenta_es(void);

// ulib.c
void u_escreve(char *s);
void u_escreve_int(char *antes, int v, char *depois);

#endif
