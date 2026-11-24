// configuração dos testes nativos (tests/nucleo.c); SUBST vem do Makefile
// (make testes compila uma vez com FIFO e outra com LRU)
#define ESCALONADOR 3
#define QUANTUM 4
#define INTERVALO 10000
#define BLOQUEIO 1
#define CARGA 2
#ifndef SUBST
#define SUBST 2
#endif
#define LIMITE_QUADROS 0
