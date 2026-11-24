// Testes nativos da lógica do núcleo; as portas e a ponte são simuladas.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/so.c"

static int recebidos[100];
static int leitura;
static int n_recebidos = 100;   // caracteres que o dispositivo entrega
static int consultas_vazias;    // consultas de estado que ainda não veem a entrada
static int escrita_pronta;
static int escrito;

int porta_le(int porta)
{
    if (porta == 2) {
        if (consultas_vazias > 0) {
            consultas_vazias--;
            return escrita_pronta;
        }
        return (leitura < n_recebidos ? 2 : 0) | escrita_pronta;
    }
    return recebidos[leitura++];
}
void porta_escreve(int porta, int valor) { assert(porta == 1); escrito = valor; }
int le_relogio(void) { return 0; }
void copia_quadro(int *origem, int *destino) { memcpy(destino, origem, 16 * sizeof(int)); }
void k_putc(int c) { (void)c; }
void k_puts(char *s) { (void)s; }
void para(void) { assert(0); }
void so_ocioso(void) {}
void fim_processo(void) {}
int programa_entrada(int prog) { return prog == PROG_INIT ? 1 : 0; }
char *programa_nome(int prog) { (void)prog; return "teste"; }

int main(void)
{
    int i;
    int quadro[16] = {0};
    for (i = 0; i < 100; i++) recebidos[i] = i;
    console_drena();
    assert(buf_n == 32 && leitura == 32);
    for (i = 0; i < 100; i++) {
        console_drena();
        assert(console_retira() == i);
    }
    assert(buf_n == 0 && leitura == 100);

    atual = -1;
    fila_ini = fila_fim = -1;
    proximo_pid = 1;
    for (i = 0; i < MAX_REG; i++) {
        assert(cria_processo(PROG_INIT) == i + 1);
        assert(tabela[0].reg == i);
        mata_processo(0);
    }
    assert(cria_processo(PROG_INIT) == -1);
    assert(n_criados == MAX_REG && n_registros == MAX_REG);
    assert(proximo_pid == MAX_REG + 1);
    assert(tabela[0].estado == P_LIVRE && fila_ini == -1);
    for (i = 0; i < MAX_REG; i++) {
        assert(registros[i].pid == i + 1 && registros[i].terminou);
    }

    // A saída real está sempre pronta; aqui exercitamos a escrita bloqueada.
    atual = 0;
    tabela[0].estado = P_EXECUTANDO;
    tabela[0].reg = -1;
    regs = quadro;
    quadro[Q_R1] = 'X';
    quantum_restante = QUANTUM;
    chamada_escreve();
    assert(atual == -1 && n_bloq_es == 1);
    assert(tabela[0].estado == P_BLOQUEADO && escrito == 0);
    escrita_pronta = 1;
    trata_pendencias();
    assert(escrito == 'X' && n_bloq_es == 0);
    assert(tabela[0].estado == P_PRONTO && tabela[0].quadro[Q_R0] == 0);

    // Leitura que bloqueia e é completada pelas pendências na mesma entrada
    // no núcleo (o caractere chega entre as duas consultas à console): o
    // processo volta a ser o escolhido e o quadro vivo precisa receber o
    // caractere gravado na tabela, não o número da chamada.
    fim_sistema = 0;
    desenfileira(0);
    atual = 0;
    tabela[0].estado = P_EXECUTANDO;
    leitura = 0;
    n_recebidos = 1;
    recebidos[0] = 'Z';
    consultas_vazias = 1;
    quadro[Q_R0] = SO_LE;
    so_trata_interrupcao(IRQ_SISTEMA, quadro);
    assert(atual == 0 && tabela[0].estado == P_EXECUTANDO && n_bloq_es == 0);
    assert(quadro[Q_R0] == 'Z');
    puts("OK: entrada sem descarte, limite de metricas, escrita bloqueada"
         " e leitura completada na mesma entrada");
    return 0;
}
