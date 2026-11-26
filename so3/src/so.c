// so.c -- núcleo do mini sistema operacional do T3 (processos,
// escalonadores, chamadas de sistema, bloqueio e preempção), escrito no
// subconjunto de C do mcc. O que o C não alcança (portas, "halt", vetor de
// interrupções, primeiro "rete") está em ponte.asm.
//
// Toda entrada no núcleo passa por so_trata_interrupcao(), que segue sempre
// o mesmo roteiro: salva o contexto do processo em execução, trata a
// interrupção, trata as pendências dos dispositivos, escalona e despacha.
//
// Configuração (gerada pelo Makefile em build/config.h):
//   ESCALONADOR  1 = simples (parte I), 2 = round-robin, 3 = prioridade
//   QUANTUM      em interrupções de relógio
//   INTERVALO    instruções entre interrupções de relógio
//   BLOQUEIO     1 = E/S bloqueia o processo (parte II), 0 = devolve -1
//   AUTOTESTE_MEM 1 = testa o gerenciador de quadros no boot
//   PRIMEIRO     programa do primeiro processo (PROG_INIT por padrão)

#include "config.h"
#include "so_chamadas.h"
#include "mem.h"
#include "programas_disco.h"

#ifndef AUTOTESTE_MEM
#define AUTOTESTE_MEM 0
#endif
#ifndef PRIMEIRO
#define PRIMEIRO PROG_INIT
#endif

#define MAX_PROC   8
#define MAX_REG    24

#define P_LIVRE       0
#define P_PRONTO      1
#define P_BLOQUEADO   2
#define P_EXECUTANDO  3

#define B_NADA     0
#define B_LE       1
#define B_ESCREVE  2
#define B_ESPERA   3

#define IRQ_SISTEMA  7
#define IRQ_CONSOLE  8
#define IRQ_RELOGIO  10

// índices no quadro de 16 palavras (ver bios_mancha/README.md)
#define Q_R0  0
#define Q_R1  1
#define Q_SP  6
#define Q_IP  7
#define Q_SR  8
#define Q_PT  12

#define SR_S  0x8000
#define SR_P  0x4000

#define PG_LEITURA 0x4000
#define PG_ESCRITA 0x8000
#define PG_TIPO    0xC000
#define PG_QUADRO  0x1FFF
#define PG_DADOS   128

// ponte.asm
int porta_le(int porta);
void porta_escreve(int porta, int valor);
int le_relogio(void);
void copia_quadro(int *origem, int *destino);
void k_putc(int c);
void k_puts(char *s);
void para(void);
void so_ocioso(void);
int fim_nucleo(void);
void zera_quadro(int q);
int disco_le(int face, int trilha, int setor, int *destino);

// ---------------------------------------------------------------- tempo
//
// Tempos são contados em instruções e passam fácil de 32767 (int do mcc
// tem 16 bits, sem unsigned/long): valor = a * 10000 + b, com 0 <= b < 10000.

struct t32 {
    int a;
    int b;
};

void t32_soma_int(struct t32 *t, int v)
{
    while (v >= 10000) {
        t->a = t->a + 1;
        v = v - 10000;
    }
    t->b = t->b + v;
    if (t->b >= 10000) {
        t->b = t->b - 10000;
        t->a = t->a + 1;
    }
}

void t32_soma(struct t32 *t, struct t32 *u)
{
    t->a = t->a + u->a;
    t32_soma_int(t, u->b);
}

// r = x - y, com x >= y
void t32_sub(struct t32 *r, struct t32 *x, struct t32 *y)
{
    r->a = x->a - y->a;
    r->b = x->b - y->b;
    if (r->b < 0) {
        r->b = r->b + 10000;
        r->a = r->a - 1;
    }
}

// r = t / n, divisão longa dígito a dígito (r*10 + dígito cabe em 16 bits
// enquanto n < 3276)
void t32_div(struct t32 *r, struct t32 *t, int n)
{
    int resto;
    int pot;
    int digito;

    r->a = t->a / n;
    resto = t->a % n;
    r->b = 0;
    pot = 1000;
    while (pot > 0) {
        digito = (t->b / pot) % 10;
        resto = resto * 10 + digito;
        r->b = r->b * 10 + resto / n;
        resto = resto % n;
        pot = pot / 10;
    }
}

// O relógio zera ao chegar em INTERVALO e gera a interrupção 10. "relogio"
// guarda as instruções até o início do período atual; o instante atual é
// relogio + contador. Se o contador virar enquanto o núcleo roda (D=1), a
// interrupção fica pendente e só chega depois do "rete": nesse caso
// percebemos a virada pela leitura menor que a anterior, adiantamos o
// período e marcamos para não somá-lo de novo quando a interrupção chegar.
struct t32 relogio;
int ultima_leitura;
int adiantado;
struct t32 agora;
int agora_lido;       // "agora" já foi lido nesta entrada no núcleo?

void le_agora(void)
{
    int c;
    if (agora_lido) {
        return;
    }
    agora_lido = 1;
    c = le_relogio();
    if (c < ultima_leitura && !adiantado) {
        t32_soma_int(&relogio, INTERVALO);
        adiantado = 1;
    }
    ultima_leitura = c;
    agora = relogio;
    t32_soma_int(&agora, c);
}

void tique_relogio(void)
{
    if (adiantado) {
        adiantado = 0;
    } else {
        t32_soma_int(&relogio, INTERVALO);
    }
    ultima_leitura = 0;
}

// ------------------------------------------------------------- console

void k_int(int v)
{
    char buf[8];
    int i;

    i = 0;
    if (v < 0) {
        k_putc('-');
        v = -v;
    }
    if (v == 0) {
        buf[0] = '0';
        i = 1;
    }
    while (v > 0) {
        buf[i] = (v % 10) + '0';
        i = i + 1;
        v = v / 10;
    }
    while (i > 0) {
        i = i - 1;
        k_putc(buf[i]);
    }
}

void k_hex(int v)
{
    char *digitos;
    int i;

    digitos = "0123456789ABCDEF";
    i = 12;
    while (i >= 0) {
        k_putc(digitos[(v >> i) & 15]);
        i = i - 4;
    }
}

void k_t32(struct t32 *t)
{
    int pot;

    if (t->a == 0) {
        k_int(t->b);
        return;
    }
    k_int(t->a);
    pot = 1000;
    while (pot > 0) {
        k_putc('0' + (t->b / pot) % 10);
        pot = pot / 10;
    }
}

int k_digitos(int v)
{
    int n;
    n = 1;
    if (v < 0) {
        n = 2;
        v = -v;
    }
    while (v >= 10) {
        v = v / 10;
        n = n + 1;
    }
    return n;
}

void k_espacos(int n)
{
    while (n > 0) {
        k_putc(' ');
        n = n - 1;
    }
}

void k_int_col(int v, int largura)
{
    k_espacos(largura - k_digitos(v));
    k_int(v);
}

void k_t32_col(struct t32 *t, int largura)
{
    if (t->a == 0) {
        k_espacos(largura - k_digitos(t->b));
    } else {
        k_espacos(largura - k_digitos(t->a) - 4);
    }
    k_t32(t);
}

void k_str_col(char *s, int largura)
{
    while (*s != 0) {
        k_putc(*s);
        s = s + 1;
        largura = largura - 1;
    }
    k_espacos(largura);
}

// Entrada: a interrupção da console chega uma vez mesmo que vários
// caracteres tenham chegado juntos. Quando o buffer enche, deixa o restante
// no dispositivo: as próximas leituras/pendências voltam a drenar a porta.
char buf_entrada[32];
int buf_ini;
int buf_n;

void console_drena(void)
{
    int c;
    while (buf_n < 32 && (porta_le(2) & 2) != 0) {
        c = porta_le(1) & 255;
        buf_entrada[(buf_ini + buf_n) % 32] = c;
        buf_n = buf_n + 1;
    }
}

int console_retira(void)
{
    int c;
    c = buf_entrada[buf_ini];
    buf_ini = (buf_ini + 1) % 32;
    buf_n = buf_n - 1;
    return c;
}

int console_pode_escrever(void)
{
    return (porta_le(2) & 1) != 0;
}

// ---------------------------------------------------------- processos

// Métricas de cada processo: ficam num vetor próprio, indexado pela ordem
// de criação, porque a entrada da tabela de processos é reaproveitada
// quando o processo morre e as métricas precisam sobreviver até o fim.
struct registro {
    int pid;
    int prog;
    int terminou;
    struct t32 criacao;
    struct t32 termino;
    int preempcoes;
    int entradas[4];
    struct t32 tempo[4];
    struct t32 resposta;
    int n_respostas;
};

struct processo {
    int estado;
    int pid;
    int prog;
    int quadro[16];
    int tp;               // quadro da tabela de páginas
    int bloqueio;
    int arg;
    int prio;             // em milésimos: 500 = 0,5
    int prox;             // próximo na fila de prontos, -1 = fim
    int reg;              // índice em registros[], -1 = sem métricas
    struct t32 t_estado;  // instante da última mudança de estado
    struct t32 t_desbloqueio;
    int mede_resposta;
};

struct processo tabela[MAX_PROC];
struct registro registros[MAX_REG];
int n_registros;

int atual;            // índice do processo em execução; -1 = nenhum

// Contexto do processo interrompido: enquanto ele continua na CPU, seus
// registradores ficam só no quadro vivo (regs) -- as chamadas de sistema
// leem argumentos e gravam resultados direto ali. A cópia para a tabela só
// acontece quando ele sai da CPU (tira_da_cpu), e a cópia de volta só
// quando outro processo entra (despacha). Isso evita duas cópias de 16
// palavras na maioria das entradas.
int *regs;
// atual no momento da entrada; -1 = ocioso; -2 = quadro vivo sem dono (boot,
// ou o interrompido já saiu da CPU e seu contexto oficial está na tabela)
int interrompido;

int n_bloq_es;        // processos bloqueados em E/S (para pular pendências)
int proximo_pid;
int fila_ini;
int fila_fim;
int quantum_restante;
int fim_sistema;

int quadro_ocioso[16];
int pilha_ocioso[8];
int ocioso;
struct t32 inicio_ocioso;
struct t32 tempo_ocioso;

int n_irq[16];
int n_chamadas[8];
int n_preempcoes;
int n_criados;

void muda_estado(struct processo *p, int novo)
{
    struct registro *r;
    struct t32 dt;

    le_agora();
    if (p->reg >= 0) {
        r = &registros[p->reg];
        t32_sub(&dt, &agora, &p->t_estado);
        t32_soma(&r->tempo[p->estado], &dt);
        if (novo != P_LIVRE) {
            r->entradas[novo] = r->entradas[novo] + 1;
        }
        if (novo == P_PRONTO && p->estado == P_BLOQUEADO) {
            p->t_desbloqueio = agora;
            p->mede_resposta = 1;
        }
        if (novo == P_EXECUTANDO && p->mede_resposta) {
            t32_sub(&dt, &agora, &p->t_desbloqueio);
            t32_soma(&r->resposta, &dt);
            r->n_respostas = r->n_respostas + 1;
            p->mede_resposta = 0;
        }
    }
    p->estado = novo;
    p->t_estado = agora;
}

void enfileira(int i)
{
    tabela[i].prox = -1;
    if (fila_fim < 0) {
        fila_ini = i;
    } else {
        tabela[fila_fim].prox = i;
    }
    fila_fim = i;
}

void desenfileira(int i)
{
    int ant;
    int k;

    ant = -1;
    k = fila_ini;
    while (k >= 0 && k != i) {
        ant = k;
        k = tabela[k].prox;
    }
    if (k < 0) {
        return;
    }
    if (ant < 0) {
        fila_ini = tabela[k].prox;
    } else {
        tabela[ant].prox = tabela[k].prox;
    }
    if (fila_fim == k) {
        fila_fim = ant;
    }
    tabela[k].prox = -1;
}

void torna_pronto(int i)
{
    muda_estado(&tabela[i], P_PRONTO);
    enfileira(i);
}

int busca_pid(int pid)
{
    int i;
    for (i = 0; i < MAX_PROC; i++) {
        if (tabela[i].estado != P_LIVRE && tabela[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

// O processo em execução perde a CPU (bloqueou ou acabou o quantum): salva
// o contexto vivo na tabela e, no escalonador por prioridade, recalcula
// prio = (prio + t_exec/t_quantum) / 2.
void tira_da_cpu(void)
{
    copia_quadro(regs, tabela[atual].quadro);
    // Daqui em diante vale a cópia da tabela: se ele for desbloqueado nesta
    // mesma entrada (resultado gravado lá) e escolhido de novo, despacha
    // precisa copiá-la de volta para o quadro vivo.
    interrompido = -2;
#if ESCALONADOR == 3
    int t_exec;
    t_exec = QUANTUM - quantum_restante;
    if (t_exec < 0) {
        t_exec = 0;
    }
    if (t_exec > QUANTUM) {
        t_exec = QUANTUM;
    }
    tabela[atual].prio = (tabela[atual].prio + (t_exec * 1000) / QUANTUM) / 2;
#endif
}

void bloqueia_atual(int motivo, int arg)
{
    struct processo *p;
    p = &tabela[atual];
    tira_da_cpu();
    p->bloqueio = motivo;
    p->arg = arg;
    if (motivo == B_LE || motivo == B_ESCREVE) {
        n_bloq_es = n_bloq_es + 1;
    }
    muda_estado(p, P_BLOQUEADO);
    atual = -1;
}

void desbloqueia(int i, int retorno)
{
    if (tabela[i].bloqueio == B_LE || tabela[i].bloqueio == B_ESCREVE) {
        n_bloq_es = n_bloq_es - 1;
    }
    tabela[i].quadro[Q_R0] = retorno;
    tabela[i].bloqueio = B_NADA;
    torna_pronto(i);
}

// ------------------------------------------------- memória dos processos
//
// Cada processo: um quadro para a tabela de páginas, as páginas de código
// do disco (só leitura), as que têm dados (globais e strings) de novo, como
// dados, e um quadro de pilha na última página de dados.

int quadros_do_processo(int prog)
{
    return prog_codigo[prog] + prog_dados[prog] + 2;
}

void libera_entrada(int *tp, int k)
{
    if (tp[k] & PG_TIPO) {
        libera_quadro(tp[k] & PG_QUADRO);
    }
}

void libera_memoria(struct processo *p, int prog)
{
    int *tp;
    int k;

    tp = end_quadro(p->tp);
    for (k = 0; k < prog_codigo[prog]; k++) {
        libera_entrada(tp, k);
    }
    for (k = 0; k < prog_dados[prog]; k++) {
        libera_entrada(tp, PG_DADOS + prog_dados_ini[prog] + k);
    }
    libera_entrada(tp, 255);
    libera_quadro(p->tp);
}

// n páginas da imagem a partir da página "pag", nas entradas desde "ent"
int carrega_paginas(int *tp, int ent, int prog, int pag, int n, int tipo)
{
    int k;
    int q;
    int lin;

    for (k = 0; k < n; k++) {
        q = aloca_quadro();
        tp[ent + k] = tipo | q;
        lin = prog_setor[prog] + pag + k;
        if (disco_le(lin / 720, (lin / 18) % 40, lin % 18, end_quadro(q)) != 0) {
            return -1;
        }
    }
    return 0;
}

int monta_memoria(struct processo *p, int prog)
{
    int *tp;
    int q;

    p->tp = aloca_quadro();
    zera_quadro(p->tp);
    tp = end_quadro(p->tp);
    q = aloca_quadro();
    zera_quadro(q);
    tp[255] = PG_ESCRITA | q;
    if (carrega_paginas(tp, 0, prog, 0, prog_codigo[prog], PG_LEITURA) < 0
        || carrega_paginas(tp, PG_DADOS + prog_dados_ini[prog], prog, prog_dados_ini[prog],
                           prog_dados[prog], PG_ESCRITA) < 0) {
        k_puts("\n[SO] erro lendo o disco\n");
        libera_memoria(p, prog);
        return -1;
    }
    return 0;
}

int cria_processo(int prog)
{
    int i;
    int k;
    struct processo *p;
    struct registro *r;

    if (prog < 0 || prog >= DISCO_N_PROGRAMAS) {
        return -1;
    }
    // Preserva as métricas de todos os processos aceitos até o fim.
    // O limite total também impede a reutilização do pid de 16 bits.
    if (n_registros >= MAX_REG) {
        return -1;
    }
    i = 0;
    while (i < MAX_PROC && tabela[i].estado != P_LIVRE) {
        i = i + 1;
    }
    if (i >= MAX_PROC || quadros_livres() < quadros_do_processo(prog)) {
        return -1;
    }

    p = &tabela[i];
    if (monta_memoria(p, prog) < 0) {
        return -1;
    }
    for (k = 0; k < 16; k++) {
        p->quadro[k] = 0;
    }
    p->quadro[Q_SP] = 0xFFFE;
    p->quadro[Q_IP] = 0;
    p->quadro[Q_SR] = SR_P;        // usuário, sem E/S, com paginação
    p->quadro[Q_PT] = (int) end_quadro(p->tp);

    p->pid = proximo_pid;
    proximo_pid = proximo_pid + 1;
    p->prog = prog;
    p->bloqueio = B_NADA;
    p->arg = 0;
    p->prio = 500;
    p->mede_resposta = 0;
    le_agora();
    p->reg = n_registros;
    r = &registros[n_registros];
    n_registros = n_registros + 1;
    r->pid = p->pid;
    r->prog = prog;
    r->criacao = agora;
    r->entradas[P_PRONTO] = 1;
    p->estado = P_PRONTO;
    p->t_estado = agora;
    enfileira(i);
    n_criados = n_criados + 1;

    k_puts("\n[SO] cria pid ");
    k_int(p->pid);
    k_puts(" (");
    k_puts(prog_nome[prog]);
    k_puts(") na entrada ");
    k_int(i);
    k_puts(", ");
    k_int(quadros_do_processo(prog));
    k_puts(" quadros\n");
    return p->pid;
}

void mata_processo(int i)
{
    struct processo *p;
    int k;
    int pid;

    p = &tabela[i];
    pid = p->pid;
    if (p->estado == P_PRONTO) {
        desenfileira(i);
    }
    if (p->estado == P_BLOQUEADO && (p->bloqueio == B_LE || p->bloqueio == B_ESCREVE)) {
        n_bloq_es = n_bloq_es - 1;
    }
    if (i == atual) {
        atual = -1;
    }
    muda_estado(p, P_LIVRE);
    libera_memoria(p, p->prog);
    if (p->reg >= 0) {
        registros[p->reg].terminou = 1;
        registros[p->reg].termino = agora;
    }
    k_puts("\n[SO] fim pid ");
    k_int(pid);
    k_puts(" (");
    k_puts(prog_nome[p->prog]);
    k_puts("), entrada ");
    k_int(i);
    k_puts(" livre\n");
    for (k = 0; k < MAX_PROC; k++) {
        if (tabela[k].estado == P_BLOQUEADO && tabela[k].bloqueio == B_ESPERA
            && tabela[k].arg == pid) {
            desbloqueia(k, 0);
        }
    }
    if (pid == 1) {
        fim_sistema = 1;
    }
}

// ------------------------------------------------------- chamadas de sistema
//
// Argumentos e resultado ficam no quadro vivo (regs) do processo que chamou.
// Se ele bloquear, bloqueia_atual salva esse quadro na tabela, e quem o
// desbloquear grava o resultado lá (desbloqueia).

void chamada_le(void)
{
    console_drena();
    if (buf_n > 0) {
        regs[Q_R0] = console_retira();
        return;
    }
#if BLOQUEIO
    bloqueia_atual(B_LE, 0);
#else
    regs[Q_R0] = -1;
#endif
}

void chamada_escreve(void)
{
    int c;
    c = regs[Q_R1] & 255;
    if (console_pode_escrever()) {
        porta_escreve(1, c);
        regs[Q_R0] = 0;
        return;
    }
#if BLOQUEIO
    bloqueia_atual(B_ESCREVE, c);
#else
    regs[Q_R0] = -1;
#endif
}

void chamada_cria(void)
{
    regs[Q_R0] = cria_processo(regs[Q_R1]);
}

void chamada_mata(void)
{
    int pid;
    int i;

    pid = regs[Q_R1];
    if (pid == 0) {
        pid = tabela[atual].pid;
    }
    i = busca_pid(pid);
    if (i < 0) {
        regs[Q_R0] = -1;
        return;
    }
    regs[Q_R0] = 0;
    mata_processo(i);
}

void chamada_espera(void)
{
    int pid;
    int i;

    pid = regs[Q_R1];
    i = busca_pid(pid);
    if (i < 0 || i == atual) {
        regs[Q_R0] = -1;
        return;
    }
    bloqueia_atual(B_ESPERA, pid);
}

void trata_chamada(void)
{
    int num;

    num = regs[Q_R0];
    if (num >= 0 && num < 8) {
        n_chamadas[num] = n_chamadas[num] + 1;
    }
    if (num == SO_LE) {
        chamada_le();
    } else if (num == SO_ESCREVE) {
        chamada_escreve();
    } else if (num == SO_CRIA_PROC) {
        chamada_cria();
    } else if (num == SO_MATA_PROC) {
        chamada_mata();
    } else if (num == SO_ESPERA_PROC) {
        chamada_espera();
    } else {
        regs[Q_R0] = -1;
    }
}

// --------------------------------------------------- outras interrupções

char *nome_irq(int irq)
{
    if (irq == 0) return "trap 0 (boot)";
    if (irq == 1) return "violacao de segmento";
    if (irq == 2) return "instrucao privilegiada";
    if (irq == 3) return "divisao por zero";
    if (irq == 4) return "instrucao ilegal";
    if (irq == 5) return "ausencia de quadro";
    if (irq == IRQ_SISTEMA) return "chamada de sistema";
    if (irq == IRQ_CONSOLE) return "console";
    if (irq == IRQ_RELOGIO) return "relogio";
    return "outra";
}

void trata_relogio(void)
{
#if ESCALONADOR != 1
    if (atual < 0) {
        return;
    }
    quantum_restante = quantum_restante - 1;
    if (quantum_restante > 0) {
        return;
    }
    if (fila_ini < 0) {
        // ninguém esperando a CPU: renova o quantum sem trocar
        quantum_restante = QUANTUM;
        return;
    }
    tira_da_cpu();
    n_preempcoes = n_preempcoes + 1;
    if (tabela[atual].reg >= 0) {
        registros[tabela[atual].reg].preempcoes = registros[tabela[atual].reg].preempcoes + 1;
    }
    torna_pronto(atual);
    atual = -1;
#endif
}

// Exceção de CPU: se veio de um processo de usuário, só ele morre; se veio
// do próprio núcleo (ou do laço ocioso), não há como continuar.
void trata_excecao(int irq, int *quadro)
{
    if (atual >= 0 && (quadro[Q_SR] & SR_S) == 0) {
        k_puts("\n[SO] processo ");
        k_int(tabela[atual].pid);
        k_puts(" (");
        k_puts(prog_nome[tabela[atual].prog]);
        k_puts(") morto: ");
        k_puts(nome_irq(irq));
        k_puts(" em ip=");
        k_hex(quadro[Q_IP]);
        k_putc('\n');
        mata_processo(atual);
        return;
    }
    k_puts("\n[SO] PANICO: ");
    k_puts(nome_irq(irq));
    k_puts(" no nucleo em ip=");
    k_hex(quadro[Q_IP]);
    k_putc('\n');
    para();
}

// Verifica os dispositivos que causaram bloqueio e completa o que ficou
// pendente, desbloqueando quem puder continuar.
void trata_pendencias(void)
{
    int i;

    if (n_bloq_es == 0) {
        return;
    }
    console_drena();
    for (i = 0; i < MAX_PROC; i++) {
        if (tabela[i].estado == P_BLOQUEADO) {
            if (tabela[i].bloqueio == B_LE && buf_n > 0) {
                desbloqueia(i, console_retira());
            } else if (tabela[i].bloqueio == B_ESCREVE && console_pode_escrever()) {
                porta_escreve(1, tabela[i].arg);
                desbloqueia(i, 0);
            }
        }
    }
}

// --------------------------------------------------------- escalonadores

#if ESCALONADOR == 1
// parte I: continua o atual se ele ainda pode rodar; senão, o primeiro
// pronto na ordem da tabela
int escolhe(void)
{
    int i;
    for (i = 0; i < MAX_PROC; i++) {
        if (tabela[i].estado == P_PRONTO) {
            return i;
        }
    }
    return -1;
}
#endif

#if ESCALONADOR == 2
// round-robin: o primeiro da fila
int escolhe(void)
{
    return fila_ini;
}
#endif

#if ESCALONADOR == 3
// prioridade: menor prio (usou menos do quantum) primeiro; empate fica com
// quem está há mais tempo na fila
int escolhe(void)
{
    int k;
    int melhor;

    melhor = fila_ini;
    k = fila_ini;
    while (k >= 0) {
        if (tabela[k].prio < tabela[melhor].prio) {
            melhor = k;
        }
        k = tabela[k].prox;
    }
    return melhor;
}
#endif

void escalona(void)
{
    int i;

    if (atual >= 0) {
        return;
    }
    i = escolhe();
    if (i < 0) {
        return;
    }
    desenfileira(i);
    atual = i;
    quantum_restante = QUANTUM;
    muda_estado(&tabela[i], P_EXECUTANDO);
}

// Põe no quadro vivo o contexto que o "rete" vai retomar. Se quem vai rodar
// é o mesmo que foi interrompido, o quadro vivo já está certo.
void despacha(int *quadro)
{
    struct t32 dt;

    if (atual >= 0) {
        if (ocioso) {
            le_agora();
            t32_sub(&dt, &agora, &inicio_ocioso);
            t32_soma(&tempo_ocioso, &dt);
            ocioso = 0;
        }
        if (atual != interrompido) {
            copia_quadro(tabela[atual].quadro, quadro);
        }
    } else {
        if (!ocioso) {
            le_agora();
            ocioso = 1;
            inicio_ocioso = agora;
        }
        if (interrompido != -1) {
            copia_quadro(quadro_ocioso, quadro);
        }
    }
}

// --------------------------------------------------------------- métricas

char *nome_escalonador(void)
{
#if ESCALONADOR == 1
    return "simples (parte I)";
#endif
#if ESCALONADOR == 2
    return "round-robin";
#endif
#if ESCALONADOR == 3
    return "prioridade";
#endif
}

void imprime_metricas(void)
{
    int i;
    int k;
    struct registro *r;
    struct t32 dt;

    le_agora();
    // fecha os intervalos ainda abertos
    for (i = 0; i < MAX_PROC; i++) {
        if (tabela[i].estado != P_LIVRE && tabela[i].reg >= 0) {
            r = &registros[tabela[i].reg];
            t32_sub(&dt, &agora, &tabela[i].t_estado);
            t32_soma(&r->tempo[tabela[i].estado], &dt);
        }
    }
    if (ocioso) {
        t32_sub(&dt, &agora, &inicio_ocioso);
        t32_soma(&tempo_ocioso, &dt);
    }

    k_puts("\n================ METRICAS DO SO ================\n");
    k_puts("escalonador: ");
    k_puts(nome_escalonador());
    k_puts(", quantum ");
    k_int(QUANTUM);
    k_puts(" x ");
    k_int(INTERVALO);
    k_puts(" instr, E/S ");
    if (BLOQUEIO) {
        k_puts("bloqueante\n");
    } else {
        k_puts("sem bloqueio\n");
    }
    k_puts("processos criados: ");
    k_int(n_criados);
    k_puts("\ntempo total:       ");
    k_t32(&agora);
    k_puts(" instr\ntempo ocioso:      ");
    k_t32(&tempo_ocioso);
    k_puts(" instr\npreempcoes:        ");
    k_int(n_preempcoes);
    k_puts("\nquadros livres:    ");
    k_int(quadros_livres());
    k_puts(" de ");
    k_int(mem_ultimo() - mem_primeiro() + 1);
    k_puts("\ninterrupcoes:\n");
    for (i = 0; i < 16; i++) {
        if (n_irq[i] > 0) {
            k_puts("  ");
            k_str_col(nome_irq(i), 24);
            k_int_col(n_irq[i], 7);
            k_putc('\n');
        }
    }
    k_puts("chamadas: le ");
    k_int(n_chamadas[SO_LE]);
    k_puts(", escreve ");
    k_int(n_chamadas[SO_ESCREVE]);
    k_puts(", cria ");
    k_int(n_chamadas[SO_CRIA_PROC]);
    k_puts(", mata ");
    k_int(n_chamadas[SO_MATA_PROC]);
    k_puts(", espera ");
    k_int(n_chamadas[SO_ESPERA_PROC]);

    k_puts("\n\npid programa  retorno preemp  resp.media (n)\n");
    for (k = 0; k < n_registros; k++) {
        r = &registros[k];
        k_int_col(r->pid, 3);
        k_putc(' ');
        k_str_col(prog_nome[r->prog], 8);
        if (r->terminou) {
            t32_sub(&dt, &r->termino, &r->criacao);
            k_t32_col(&dt, 9);
        } else {
            k_puts("    (vivo)");
        }
        k_int_col(r->preempcoes, 7);
        if (r->n_respostas > 0) {
            t32_div(&dt, &r->resposta, r->n_respostas);
            k_t32_col(&dt, 12);
        } else {
            k_puts("           -");
        }
        k_puts(" (");
        k_int(r->n_respostas);
        k_puts(")\n");
    }

    k_puts("\n            vezes em estado     tempo em estado (instr)\n");
    k_puts("pid programa pron bloq exec     pronto  bloqueado   executando\n");
    for (k = 0; k < n_registros; k++) {
        r = &registros[k];
        k_int_col(r->pid, 3);
        k_putc(' ');
        k_str_col(prog_nome[r->prog], 8);
        k_int_col(r->entradas[P_PRONTO], 5);
        k_int_col(r->entradas[P_BLOQUEADO], 5);
        k_int_col(r->entradas[P_EXECUTANDO], 5);
        k_t32_col(&r->tempo[P_PRONTO], 11);
        k_t32_col(&r->tempo[P_BLOQUEADO], 11);
        k_t32_col(&r->tempo[P_EXECUTANDO], 13);
        k_putc('\n');
    }
    k_puts("================================================\n");
}

// --------------------------------------------------------- pontos de entrada

void so_trata_interrupcao(int irq, int *quadro)
{
    agora_lido = 0;
    if (irq == IRQ_RELOGIO) {
        tique_relogio();
    }
    n_irq[irq] = n_irq[irq] + 1;
    regs = quadro;
    interrompido = atual;

    if (irq == IRQ_SISTEMA) {
        trata_chamada();
    } else if (irq == IRQ_RELOGIO) {
        trata_relogio();
    } else if (irq == IRQ_CONSOLE) {
        console_drena();
    } else {
        trata_excecao(irq, quadro);
    }

    trata_pendencias();

    if (fim_sistema) {
        imprime_metricas();
        para();
    }

    escalona();
    despacha(quadro);
}

// chamada uma vez por _kernel_inicio (ponte.asm), com D=1
#if AUTOTESTE_MEM
void autoteste_mem(void)
{
    int visto[MAX_QUADROS];
    int total;
    int n;
    int q;
    int i;
    int *p;

    total = quadros_livres();
    for (q = 0; q < MAX_QUADROS; q++) {
        visto[q] = 0;
    }
    n = 0;
    q = aloca_quadro();
    while (q >= 0) {
        if (q < mem_primeiro() || q > mem_ultimo() || visto[q]) {
            k_puts("[SO] PANICO: autoteste memoria: quadro ");
            k_int(q);
            k_puts(" fora da faixa ou repetido\n");
            para();
        }
        visto[q] = 1;
        n = n + 1;
        q = aloca_quadro();
    }
    if (n != total || quadros_livres() != 0) {
        k_puts("[SO] PANICO: autoteste memoria: contagem errada\n");
        para();
    }

    p = end_quadro(mem_primeiro());
    for (i = 0; i < TAM_QUADRO / 2; i++) {
        p[i] = 0x5A5A;
    }
    zera_quadro(mem_primeiro());
    for (i = 0; i < TAM_QUADRO / 2; i++) {
        if (p[i] != 0) {
            k_puts("[SO] PANICO: autoteste memoria: zera_quadro\n");
            para();
        }
    }

    for (q = mem_primeiro(); q <= mem_ultimo(); q++) {
        libera_quadro(q);
    }
    if (quadros_livres() != total) {
        k_puts("[SO] PANICO: autoteste memoria: livres nao voltou ao inicial\n");
        para();
    }
    k_puts("[SO] autoteste memoria: ok (");
    k_int(n);
    k_puts(" quadros alocados, zerados e liberados)\n");
}
#endif

void inicia_memoria(void)
{
    int fim;
    int primeiro;

    fim = fim_nucleo();
    primeiro = (fim >> 9) & 127;
    if (fim & (TAM_QUADRO - 1)) {
        primeiro = primeiro + 1;
    }
    if (primeiro >= QUADRO_LIMITE) {
        k_puts("[SO] PANICO: o nucleo invade a area das pilhas\n");
        para();
    }
    mem_inicia(primeiro, QUADRO_LIMITE - 1);
    k_puts("[SO] memoria: quadros ");
    k_int(mem_primeiro());
    k_puts(" a ");
    k_int(mem_ultimo());
    k_puts(" livres (");
    k_int(quadros_livres());
    k_puts(" quadros de 512 bytes)\n");
#if AUTOTESTE_MEM
    autoteste_mem();
#endif
}

void so_inicia(int *quadro)
{
    int i;
    void (*f)(void);

    atual = -1;
    interrompido = -2;
    regs = quadro;
    fila_ini = -1;
    fila_fim = -1;
    proximo_pid = 1;
    for (i = 0; i < MAX_PROC; i++) {
        tabela[i].estado = P_LIVRE;
    }

    f = so_ocioso;
    quadro_ocioso[Q_SP] = (int) &pilha_ocioso[8];
    quadro_ocioso[Q_IP] = (int) f;
    quadro_ocioso[Q_SR] = 0xA000;   // S=1, I=1, D=0

    // relógio: interrupção 10 a cada INTERVALO instruções; máscara do
    // controlador libera console (bit 0) e relógio (bit 2)
    porta_escreve(0x22, (INTERVALO >> 8) & 255);
    porta_escreve(0x23, INTERVALO & 255);
    porta_escreve(0x30, 5);

    le_agora();
    k_puts("[SO] iniciando: escalonador ");
    k_puts(nome_escalonador());
    k_puts(", quantum ");
    k_int(QUANTUM);
    k_puts(", relogio a cada ");
    k_int(INTERVALO);
    k_puts(" instr\n");
    inicia_memoria();

    if (cria_processo(PRIMEIRO) < 0) {
        k_puts("[SO] PANICO: nao foi possivel criar o init\n");
        para();
    }
    escalona();
    despacha(quadro);
}
