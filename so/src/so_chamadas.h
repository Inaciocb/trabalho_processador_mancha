// so_chamadas.h -- números das chamadas de sistema, compartilhados entre o
// núcleo (src/so.c) e os programas de usuário (usuario/). Convenção da BIOS:
// número em r0, argumentos em r1..r4, "trap 7", resultado em r0.
//
// Os mesmos valores estão repetidos em usuario/runtime_so.asm (o montador
// não lê cabeçalhos C) -- mantenha os dois em sincronia.

#ifndef SO_CHAMADAS_H
#define SO_CHAMADAS_H

#define SO_LE           1   // r0 = caractere lido
#define SO_ESCREVE      2   // r1 = caractere; r0 = 0
#define SO_CRIA_PROC    3   // r1 = número do programa; r0 = pid ou -1
#define SO_MATA_PROC    4   // r1 = pid (0 = o próprio); r0 = 0 ou -1
#define SO_ESPERA_PROC  5   // r1 = pid; r0 = 0 (morreu) ou -1 (erro)

// programas conhecidos pelo SO (usuario/programas.c, tabela_programas)
#define PROG_INIT     0
#define PROG_CALC     1
#define PROG_ECO      2
#define PROG_MISTO    3
#define PROG_ETERNO   4
#define PROG_INVASOR  5
#define PROG_OLA      6
#define N_PROGRAMAS   7

#endif
