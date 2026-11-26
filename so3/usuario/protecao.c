// protecao: primeiro processo do cenário de proteção (make protecao). Roda
// cada teste de acesso inválido, um por vez, e depois dois gemeo juntos.

#include "lib_usuario.c"

void testa(char *nome, int prog)
{
    int pid;

    pid = so_cria_proc(prog);
    u_escreve("protecao: ");
    u_escreve(nome);
    u_escreve_int(" pid=", pid, "");
    u_escreve_int(", espera = ", so_espera_proc(pid), "\n");
}

int main(void)
{
    int a;
    int b;

    testa("vetor", PROG_VETOR);
    testa("autocod", PROG_AUTOCOD);
    testa("salto", PROG_SALTO);
    testa("pilha", PROG_PILHA);

    a = so_cria_proc(PROG_GEMEO);
    b = so_cria_proc(PROG_GEMEO);
    u_escreve_int("protecao: gemeos pid=", a, "");
    u_escreve_int(" e ", b, "\n");
    so_espera_proc(a);
    so_espera_proc(b);
    u_escreve("protecao: fim\n");
    return 0;
}
