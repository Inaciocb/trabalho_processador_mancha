// Testes nativos (gcc) da lógica do núcleo do T3; as portas, a ponte, a
// memória física e o disco são simulados. Compilado duas vezes pelo
// Makefile (make testes): com SUBST=1 (FIFO) e SUBST=2 (LRU).
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/so.c"

// ------------------------------------------------------------ simulações

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

// memória física (palavras) e disco (setores de 256 palavras)
static int mem[65536 / 2];
static int disco[SETORES_DISCO][256];
static int n_leituras;
static int ultima_escrita = -1;

int pt_le(int q, int i) { return mem[q * 256 + i]; }
void pt_escreve(int q, int i, int v) { mem[q * 256 + i] = v; }
void zera_quadro(int q) { memset(&mem[q * 256], 0, 256 * sizeof(int)); }
int disco_le(int s, int q) { memcpy(&mem[q * 256], disco[s], sizeof disco[s]); n_leituras++; return 0; }
int disco_escreve(int s, int q) { memcpy(disco[s], &mem[q * 256], sizeof disco[s]); ultima_escrita = s; return 0; }
int so_fim_imagem(void) { return 60 * 512 + 100; }   // pool começa no quadro 61

// referência em C do envelhece() de ponte.asm (mesma lógica)
void envelhece(void)
{
    int q = fifo_ini;
    while (q >= 0) {
        int a = quadro_ent[q] / 2;
        int idade = quadro_idade[q] >> 1;
        if (mem[a] & ACESSO) { idade |= 128; mem[a] &= ~ACESSO; }
        if (mem[a + 128] & ACESSO) { idade |= 128; mem[a + 128] &= ~ACESSO; }
        quadro_idade[q] = idade;
        q = quadro_prox[q];
    }
}

// ------------------------------------------------------------ auxiliares

static int quadro_vivo[16];

static void reinicia(void)
{
    int i;
    memset(tabela, 0, sizeof tabela);
    memset(registros, 0, sizeof registros);
    n_registros = 0;
    for (i = 0; i < MAX_PROC; i++) tabela[i].estado = P_LIVRE;
    atual = -1;
    fila_ini = fila_fim = -1;
    proximo_pid = 1;
    n_bloq_es = 0;
    fim_sistema = 0;
    n_faltas = n_substituicoes = n_escritas = 0;
    inicia_quadros();
}

// põe a entrada i para rodar, com o quadro vivo dela em quadro_vivo
static void executa(int i)
{
    if (tabela[i].estado == P_PRONTO) desenfileira(i);
    tabela[i].estado = P_EXECUTANDO;
    atual = i;
    interrompido = i;
    memcpy(quadro_vivo, tabela[i].quadro, sizeof quadro_vivo);
    regs = quadro_vivo;
}

// falta de página na entrada i com o ip dado (trata_falta direto)
static void falta(int i, int ip)
{
    executa(i);
    quadro_vivo[Q_IP] = ip;
    trata_falta(quadro_vivo);
}

static int presente(int i, int pag) { return TIPO(pt_le(tabela[i].pt, pag)) != 0; }

// ------------------------------------------------------------ testes do T2

static void testes_t2(void)
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

    reinicia();
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
    assert(n_usados == 0);              // 24 criações e mortes sem vazar quadro

    // A saída real está sempre pronta; aqui exercitamos a escrita bloqueada.
    reinicia();
    assert(cria_processo(PROG_INIT) == 1);
    atual = 0;
    desenfileira(0);
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
    // no núcleo: o quadro vivo precisa receber o caractere gravado na
    // tabela, não o número da chamada.
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
}

// ------------------------------------------------------------ testes do T3

// criação: espaço de endereçamento e quadro inicial; morte sem vazamento
static void teste_criacao(void)
{
    int pt, pag, livres;

    reinicia();
    assert(primeiro_quadro == 61 && n_pool == FIM_POOL - 61);
    assert(quadro_ocupado(60) && quadro_ocupado(FIM_POOL) && quadro_ocupado(2047));
    livres = quadros_livres();
    assert(cria_processo(PROG_CALC) == 1);
    pt = tabela[0].pt;
    assert(n_usados == 1 + 1 + 2);                         // tabela + dados + pilha
    assert(tabela[0].quadro[Q_IP] == 0 && tabela[0].quadro[Q_SP] == 0);
    assert(tabela[0].quadro[Q_SR] == SR_P);                // S=0, P=1, I=0
    assert(tabela[0].quadro[Q_PT] == pt * 512);
    for (pag = 0; pag < 128; pag++) {
        assert(pt_le(pt, pag) == 0);                       // código por demanda
        if (pag == 4 || pag >= PAG_PILHA) {
            assert(TIPO(pt_le(pt, 128 + pag)) == 3);       // dados e pilha
        } else {
            assert(pt_le(pt, 128 + pag) == 0);             // proteção
        }
    }
    // dados vieram do disco: setor prog_setor[1] + 4
    disco[9][0] = 1234;
    mata_processo(0);
    assert(quadros_livres() == livres && n_usados == 0 && fifo_ini == -1);
    assert(cria_processo(PROG_CALC) == 2);
    assert(mem[QUADRO(pt_le(tabela[0].pt, 128 + 4)) * 256] == 1234);
    mata_processo(0);
    assert(n_usados == 0);
}

// SO_CRIA_PROC sem memória: devolve -1 e desfaz as alocações parciais
static void teste_criacao_sem_memoria(void)
{
    reinicia();
    limite_quadros = 3;                  // tabela + dados cabem, a pilha não
    assert(cria_processo(PROG_CALC) == -1);
    assert(n_usados == 0 && tabela[0].estado == P_LIVRE && n_registros == 0);
    limite_quadros = 4;
    assert(cria_processo(PROG_CALC) == 1);
    assert(n_usados == 4);
}

// faltas: legítima de código, cruzamento, dado, código fora do programa
static void teste_faltas(void)
{
    int q;

    reinicia();
    assert(cria_processo(PROG_CALC) == 1);
    assert(cria_processo(PROG_CALC) == 2);

    // legítima: página 1 carregada só leitura nas duas entradas, ip intacto
    disco[5 + 1][0] = 777;
    falta(0, 0x0234);
    assert(tabela[0].estado == P_EXECUTANDO && quadro_vivo[Q_IP] == 0x0234);
    q = QUADRO(pt_le(tabela[0].pt, 1));
    assert(TIPO(pt_le(tabela[0].pt, 1)) == 1 && pt_le(tabela[0].pt, 128 + 1) == pt_le(tabela[0].pt, 1));
    assert(mem[q * 256] == 777 && quadro_uso[q] == Q_RO && quadro_dono[q] == 0);
    assert(n_faltas == 1 && registros[0].faltas == 1);
    assert(!presente(1, 1));             // o outro processo (mesmo programa) não vê

    // cruzamento: falta em 2*512 com a página 2 listada -> ip volta 2
    falta(0, 0x0400);
    assert(quadro_vivo[Q_IP] == 0x03FE && presente(0, 2) && pagina_fixa == -1);
    // sem cruzamento listado (página 3): ip intacto
    falta(0, 0x0600);
    assert(quadro_vivo[Q_IP] == 0x0600 && presente(0, 3));

    // falta de dado: a página do ip já está presente -> morre
    falta(0, 0x0240);
    assert(tabela[0].estado == P_LIVRE && atual == -1);
    assert(n_faltas == 3);

    // código fora do programa (página 48 >= n_ro) -> morre
    falta(1, 0x6000);
    assert(tabela[1].estado == P_LIVRE);
    // dados/pilha como código também (página 4 = dados)
    assert(cria_processo(PROG_CALC) == 3);
    falta(0, 0x0800);
    assert(tabela[0].estado == P_LIVRE);
    assert(n_usados == 0 && fifo_ini == -1);
}

// substituição: vítima, anulação na tabela do DONO, página fixada
static void teste_substituicao(void)
{
    int q0;

    reinicia();
    assert(cria_processo(PROG_CALC) == 1);     // entrada 0: 4 quadros
    assert(cria_processo(PROG_CALC) == 2);     // entrada 1: 4 quadros
    limite_quadros = 8 + 3;                     // sobram 3 para código

    falta(0, 0x0000);                           // e0 pag 0 (1º da fila)
    q0 = QUADRO(pt_le(tabela[0].pt, 0));
    falta(0, 0x0200);                           // e0 pag 1
    falta(1, 0x0000);                           // e1 pag 0
    assert(quadros_livres() == 0 && n_substituicoes == 0);

#if SUBST == 1
    // FIFO: a vítima é a mais antiga (e0 pag 0), anulada na tabela de e0,
    // embora a falta seja de e1
    falta(1, 0x0200);
    assert(n_substituicoes == 1 && !presente(0, 0) && pt_le(tabela[0].pt, 128) == 0);
    assert(presente(0, 1) && presente(1, 0) && presente(1, 1));
    assert(QUADRO(pt_le(tabela[1].pt, 1)) == q0);   // o quadro foi reaproveitado
#else
    // LRU: e0 pag 0 e e1 pag 0 foram acessadas; e0 pag 1 não. Dois tiques:
    // a não acessada envelhece e vira a vítima, mesmo não sendo a mais antiga
    envelhece();
    mem[quadro_ent[q0] / 2] |= ACESSO;                                   // busca (código)
    mem[quadro_ent[QUADRO(pt_le(tabela[1].pt, 0))] / 2 + 128] |= ACESSO; // leitura (espelho)
    envelhece();
    falta(1, 0x0200);
    assert(n_substituicoes == 1 && !presente(0, 1) && presente(0, 0) && presente(1, 0));
    // empate de idades: vence a mais antiga na fila
    for (int i = 0; i < 8; i++) envelhece();   // todas chegam a 0
    falta(1, 0x0600);
    assert(n_substituicoes == 2 && !presente(0, 0));
#endif

    // página fixada: falta com cruzamento em e1 (pag 2): a pag 1 de e1 não
    // pode sair, mesmo sendo a vítima natural
    reinicia();
    assert(cria_processo(PROG_CALC) == 1);
    limite_quadros = 4 + 2;
    falta(0, 0x0200);                           // pag 1 (1ª da fila)
    falta(0, 0x0000);                           // pag 0
    falta(0, 0x0400);                           // pag 2, cruzamento: fixa a 1
    assert(quadro_vivo[Q_IP] == 0x03FE);
    assert(presente(0, 1) && presente(0, 2) && !presente(0, 0));
}

// escrita de volta: página modificada (tipo 3) vai para a área de troca do
// dono antes de o quadro ser reaproveitado; tipos 1/2 são descartados
static void teste_escrita_de_volta(void)
{
    int q;

    reinicia();
    assert(cria_processo(PROG_CALC) == 1);
    assert(cria_processo(PROG_CALC) == 2);
    limite_quadros = 8 + 1;
    falta(1, 0x0200);                           // e1 pag 1 no único quadro de código
    q = QUADRO(pt_le(tabela[1].pt, 1));
    mem[q * 256 + 5] = 4321;
    // simula a página marcada como modificada (só código é vítima, então
    // isso não acontece no cenário real: ver README)
    pt_escreve(tabela[1].pt, 128 + 1, ENTRADA(3, q));
    falta(0, 0x0000);
    assert(n_substituicoes == 1 && n_escritas == 1);
    assert(ultima_escrita == SETOR_TROCA + 1 * SETORES_TROCA + 1);
    assert(disco[ultima_escrita][5] == 4321);
    assert(pt_le(tabela[1].pt, 1) == 0 && pt_le(tabela[1].pt, 128 + 1) == 0);
    // tipo 1: descarta sem escrever
    falta(1, 0x0200);
    assert(n_substituicoes == 2 && n_escritas == 1);
}

int main(void)
{
    testes_t2();
    teste_criacao();
    teste_criacao_sem_memoria();
    teste_faltas();
    teste_substituicao();
    teste_escrita_de_volta();
    printf("OK (SUBST=%d): entrada sem descarte, limite de metricas, escrita bloqueada,"
           " leitura completada; criacao/morte sem vazamento, criacao sem memoria"
           " desfeita, faltas (codigo, cruzamento, dado, fora do codigo),"
           " vitima %s, pagina fixada, anulacao no dono, escrita de volta\n",
           SUBST, SUBST == 1 ? "FIFO" : "LRU");
    return 0;
}
