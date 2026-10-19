// programas.c -- programas de usuário que rodam sobre o SO, e a tabela que
// o núcleo usa para achar o ponto de entrada de cada um (SO_CRIA_PROC
// recebe o número do programa). Todos rodam em modo usuário (S=0, I=0) e
// só falam com o mundo por chamadas de sistema.
//
// Cenário de teste: o init cria processos com perfis diferentes, espera
// cada um morrer, mata o que nunca termina e sai -- o que encerra o SO e
// imprime as métricas.

#include <mancha.h>
#include "so.h"

#define CALC_LIMITE   900
#define MISTO_PASSOS  6
#define MISTO_CONTA   1500
#define ECO_LINHAS    3

void init(void);
void calc(void);
void eco(void);
void misto(void);
void eterno(void);
void invasor(void);
void ola(void);

void (*tabela_programas[N_PROGRAMAS])(void) = { init, calc, eco, misto, eterno, invasor, ola };

int programa_entrada(int prog)
{
    if (prog < 0 || prog >= N_PROGRAMAS) {
        return 0;
    }
    return (int) tabela_programas[prog];
}

char *programa_nome(int prog)
{
    if (prog == PROG_INIT) return "init";
    if (prog == PROG_CALC) return "calc";
    if (prog == PROG_ECO) return "eco";
    if (prog == PROG_MISTO) return "misto";
    if (prog == PROG_ETERNO) return "eterno";
    if (prog == PROG_INVASOR) return "invasor";
    if (prog == PROG_OLA) return "ola";
    return "?";
}

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

// init: cria os outros, testa os erros de SO_ESPERA_PROC, espera todos e sai
void init(void)
{
    int calc_pid;
    int eco_pid;
    int misto_pid;
    int eterno_pid;
    int invasor_pid;
    int ola_pid;

    u_escreve("init: criando processos\n");
    calc_pid = so_cria_proc(PROG_CALC);
    eco_pid = so_cria_proc(PROG_ECO);
    misto_pid = so_cria_proc(PROG_MISTO);
    eterno_pid = so_cria_proc(PROG_ETERNO);
    invasor_pid = so_cria_proc(PROG_INVASOR);
    u_escreve_int("init: calc=", calc_pid, "");
    u_escreve_int(" eco=", eco_pid, "");
    u_escreve_int(" misto=", misto_pid, "");
    u_escreve_int(" eterno=", eterno_pid, "");
    u_escreve_int(" invasor=", invasor_pid, "\n");

    u_escreve_int("init: espera(1, o proprio) = ", so_espera_proc(1), "\n");
    u_escreve_int("init: espera(99, inexistente) = ", so_espera_proc(99), "\n");
    u_escreve_int("init: cria(programa 42) = ", so_cria_proc(42), "\n");

    // 0 = esperou o processo morrer; -1 = ele já tinha morrido antes
    u_escreve_int("init: espera(misto) = ", so_espera_proc(misto_pid), "\n");
    u_escreve_int("init: mata(eterno) = ", so_mata_proc(eterno_pid), "\n");
    u_escreve_int("init: espera(eterno, ja morto) = ", so_espera_proc(eterno_pid), "\n");
    u_escreve_int("init: espera(calc) = ", so_espera_proc(calc_pid), "\n");
    u_escreve_int("init: espera(eco) = ", so_espera_proc(eco_pid), "\n");

    // as entradas da tabela já foram liberadas: o novo processo reaproveita
    // uma delas, mas recebe um pid nunca usado
    ola_pid = so_cria_proc(PROG_OLA);
    u_escreve_int("init: ola=", ola_pid, "\n");
    u_escreve_int("init: espera(ola) = ", so_espera_proc(ola_pid), "\n");
    u_escreve("init: fim\n");
}

// CPU-bound: conta primos por divisão, sem nenhuma chamada no meio
void calc(void)
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
}

// I/O-bound: lê linhas da console (bloqueia esperando o operador) e as
// devolve em maiúsculas
void eco(void)
{
    char linha[40];
    int n;
    int c;
    int lidas;

    u_escreve("eco: esperando 3 linhas\n");
    lidas = 0;
    n = 0;
    while (lidas < ECO_LINHAS) {
        c = getchar();
        if (c == '\n') {
            linha[n] = 0;
            u_escreve("eco: ");
            u_escreve(linha);
            putchar('\n');
            n = 0;
            lidas = lidas + 1;
        } else if (n < 39) {
            if (c >= 'a' && c <= 'z') {
                c = c - 32;
            }
            linha[n] = c;
            n = n + 1;
        }
    }
}

// alterna um pouco de cálculo com escrita
void misto(void)
{
    int passo;
    int i;
    int x;

    x = 0;
    for (passo = 1; passo <= MISTO_PASSOS; passo++) {
        for (i = 0; i < MISTO_CONTA; i++) {
            x = x + i;
        }
        u_escreve_int("misto: passo ", passo, "\n");
    }
}

// nunca termina: o init o mata no fim
void eterno(void)
{
    int x;
    x = 0;
    for (;;) {
        x = x + 1;
    }
}

// tenta usar a console sem passar pelo SO
void invasor(void)
{
    u_escreve("invasor: tentando E/S direta\n");
    tenta_es();
    u_escreve("invasor: isto nao deveria aparecer\n");
}

// processo curto criado no fim pelo init
void ola(void)
{
    u_escreve("ola: processo novo numa entrada reaproveitada\n");
}
