// init.c -- primeiro processo (programa 0). Cria os demais, testa os erros
// das chamadas (como no T2), espera, mata o que nunca termina, roda os
// testes de proteção de memória um de cada vez e por fim cria o "ola"
// numa entrada reaproveitada. Quando ele sai, o SO imprime as métricas.
//
// Os processos são criados em duas levas para o cenário caber na memória
// também na parte I (carga completa), em que cada processo ocupa todas as
// suas páginas desde a criação (ver README).

#include <mancha.h>
#include "so.h"

// cria e avisa se o SO recusou (falta de memória ou de entrada na tabela):
// o relatório usa a mensagem "ERRO" para saber que o cenário não concluiu
int cria(int prog, char *nome)
{
    int pid;
    pid = so_cria_proc(prog);
    if (pid < 0) {
        u_escreve("init: ERRO ao criar ");
        u_escreve(nome);
        putchar('\n');
    }
    return pid;
}

// cria um processo e espera ele terminar (os testes de proteção morrem logo)
void roda(int prog, char *nome)
{
    int pid;
    pid = cria(prog, nome);
    if (pid >= 0) {
        so_espera_proc(pid);
    }
}

int main(void)
{
    int calc1;
    int calc2;
    int eco_pid;
    int misto_pid;
    int eterno_pid;
    int invasor_pid;
    int paginas_pid;
    int ola_pid;

    // 1ª leva: o mesmo programa (o mesmo binário no disco) em dois
    // processos, mais E/S e um misto
    u_escreve("init: criando processos\n");
    calc1 = cria(PROG_CALC, "calc");
    calc2 = cria(PROG_CALC, "calc");
    eco_pid = cria(PROG_ECO, "eco");
    misto_pid = cria(PROG_MISTO, "misto");
    u_escreve_int("init: calc=", calc1, "");
    u_escreve_int(",", calc2, "");
    u_escreve_int(" eco=", eco_pid, "");
    u_escreve_int(" misto=", misto_pid, "\n");

    u_escreve_int("init: espera(1, o proprio) = ", so_espera_proc(1), "\n");
    u_escreve_int("init: espera(99, inexistente) = ", so_espera_proc(99), "\n");
    u_escreve_int("init: cria(programa 42) = ", so_cria_proc(42), "\n");

    // 0 = esperou o processo morrer; -1 = ele já tinha morrido antes
    u_escreve_int("init: espera(misto) = ", so_espera_proc(misto_pid), "\n");
    u_escreve_int("init: espera(calc) = ", so_espera_proc(calc1), "\n");
    u_escreve_int("init: espera(calc) = ", so_espera_proc(calc2), "\n");
    u_escreve_int("init: espera(eco) = ", so_espera_proc(eco_pid), "\n");

    // 2ª leva: código grande, um que nunca termina e um invasor
    eterno_pid = cria(PROG_ETERNO, "eterno");
    invasor_pid = cria(PROG_INVASOR, "invasor");
    paginas_pid = cria(PROG_PAGINAS, "paginas");
    u_escreve_int("init: eterno=", eterno_pid, "");
    u_escreve_int(" invasor=", invasor_pid, "");
    u_escreve_int(" paginas=", paginas_pid, "\n");
    u_escreve_int("init: espera(paginas) = ", so_espera_proc(paginas_pid), "\n");
    u_escreve_int("init: mata(eterno) = ", so_mata_proc(eterno_pid), "\n");
    u_escreve_int("init: espera(eterno, ja morto) = ", so_espera_proc(eterno_pid), "\n");

    // proteção de memória: cada um deve morrer sem afetar o resto
    u_escreve("init: testes de protecao\n");
    roda(PROG_ESCREVE_CODIGO, "escreve_codigo");
    roda(PROG_LE_LONGE, "le_longe");
    roda(PROG_SALTA_LONGE, "salta_longe");

    // as entradas da tabela já foram liberadas: o novo processo reaproveita
    // uma delas, mas recebe um pid nunca usado
    ola_pid = cria(PROG_OLA, "ola");
    u_escreve_int("init: ola=", ola_pid, "\n");
    u_escreve_int("init: espera(ola) = ", so_espera_proc(ola_pid), "\n");
    u_escreve("init: fim\n");
    return 0;
}
