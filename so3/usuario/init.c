// init: cria os outros, testa os erros de SO_ESPERA_PROC, espera todos e sai
//
// Cenário de teste: o init cria processos com perfis diferentes, espera
// cada um morrer, mata o que nunca termina e sai -- o que encerra o SO e
// imprime as métricas.

#include "lib_usuario.c"

int main(void)
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
    return 0;
}
