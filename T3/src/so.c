// so.c -- núcleo do mini sistema operacional do T3: o do T2 (processos,
// escalonadores, chamadas de sistema, bloqueio e preempção) com processos
// de usuário isolados por paginação, carregados do disco simulado (parte
// I), código carregado por demanda (parte II) e substituição de páginas
// FIFO/LRU (parte III). Escrito no subconjunto de C do mcc. O que o C não
// alcança (portas, "halt", vetor de interrupções, primeiro "rete", acesso
// à memória física e ao disco) está em ponte.asm.
//
// Toda entrada no núcleo passa por so_trata_interrupcao(), que segue sempre
// o mesmo roteiro: salva o contexto do processo em execução, trata a
// interrupção, trata as pendências dos dispositivos, escalona e despacha.
//
// Configuração (gerada pelo Makefile em build/.../config.h):
//   ESCALONADOR     1 = simples, 2 = round-robin, 3 = prioridade
//   QUANTUM         em interrupções de relógio
//   INTERVALO       instruções entre interrupções de relógio
//   BLOQUEIO        1 = E/S bloqueia o processo, 0 = devolve -1
//   CARGA           1 = programa inteiro na criação, 2 = código por demanda
//   SUBST           0 = sem substituição, 1 = FIFO, 2 = LRU (só com CARGA=2)
//   LIMITE_QUADROS  máximo de quadros para processos (0 = o pool todo)
//
// programas_disco.h (gerado pelo mkdisco) diz onde cada programa está no
// disco, quantas páginas de código (n_ro) e de dados (n_rw) ele tem e em
// quais páginas há instruções que cruzam a fronteira (ver trata_falta).

#include "config.h"
#include "so_chamadas.h"
#include "programas_disco.h"

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
#define Q_PT  12      // s4: sob paginação, endereço da tabela de páginas

#define SR_S  0x8000
#define SR_P  0x4000

// ponte.asm
int porta_le(int porta);
void porta_escreve(int porta, int valor);
int le_relogio(void);
void copia_quadro(int *origem, int *destino);
void k_putc(int c);
void k_puts(char *s);
void para(void);
void so_ocioso(void);

// Memória física e disco (ponte.asm). No C só circulam NÚMEROS DE QUADRO
// (0..127) e índices: um endereço físico >= 0x8000 seria negativo no int
// de 16 bits do mcc, e a multiplicação por 512 fica no assembly.
int pt_le(int q_tabela, int indice);                 // palavra em q*512 + indice*2
void pt_escreve(int q_tabela, int indice, int valor);
void zera_quadro(int q);                              // 256 palavras
int disco_le(int setor_linear, int q);                // 0 = ok
int disco_escreve(int setor_linear, int q);           // 0 = ok
int so_fim_imagem(void);                              // fim da imagem do núcleo (fim.asm)

// LRU por envelhecimento (parte III), chamado a cada tique: percorre a fila
// dos quadros de código e, em cada um, faz idade = (idade >> 1) | 128 se a
// página foi acessada desde o último tique (bit de acesso da entrada de
// código ou do espelho de dados, ligado pela CPU), desligando o bit. Em
// assembly porque roda a cada tique: em C custava ~60 instruções por
// quadro. Usa fifo_ini, quadro_prox, quadro_idade e quadro_ent.
void envelhece(void);

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
    int faltas;           // faltas de página atendidas (parte II/III)
};

struct processo {
    int estado;
    int pid;
    int prog;
    int quadro[16];
    int bloqueio;
    int arg;
    int prio;             // em milésimos: 500 = 0,5
    int prox;             // próximo na fila de prontos, -1 = fim
    int reg;              // índice em registros[], -1 = sem métricas
    struct t32 t_estado;  // instante da última mudança de estado
    struct t32 t_desbloqueio;
    int mede_resposta;
    int pt;               // quadro da tabela de páginas
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

// ------------------------------------------------------ memória e quadros
//
// Endereço virtual de 16 bits = página (7 bits) + deslocamento (9 bits).
// Uma tabela de páginas tem 256 entradas: 0..127 para busca de instrução,
// 128..255 para acesso a dados, indexadas pela mesma página virtual.
// Entrada = (tipo << 14) | (acesso << 13) | quadro. Nunca comparar entradas
// com < ou >: tipos 2 e 3 são negativos no int de 16 bits.
//
// Espaço virtual de todo processo (o programa foi montado em duas partes
// alinhadas a página, ver o Makefile e o mkdisco):
//   páginas 0 .. n_ro-1          código: entrada de código tipo 1 e a de
//                                dados tipo 1 no MESMO quadro (espelho só
//                                leitura: escrever no código dá a int 1)
//   páginas n_ro .. n_ro+n_rw-1  dados (.data, strings, globais): tipo 3
//   páginas 126 e 127            pilha (sp começa em 0: o 1º push vai a 0xFFFE)
//   o resto                      tipo 0: qualquer acesso dá a int 5

#define TIPO(e)       (((e) >> 14) & 3)
#define ACESSO        0x2000
#define QUADRO(e)     ((e) & 0x1FFF)
#define ENTRADA(t, q) (((t) << 14) | (q))

#define PAG_PILHA      126        // páginas 126 e 127
#define QUADROS_REAIS  2048       // 1 MB / 512
#define MAPA_PALAVRAS  128        // 2048 bits
#define QUADROS_ALC    128        // só os primeiros 64 KB são alcançáveis (README)
#define FIM_POOL       104        // 0xD000/512: daí para cima, pilhas do núcleo e do sistema
#define SETORES_DISCO  1440
#define SETORES_TROCA  128        // por entrada da tabela de processos (uma página = um setor)

// uso de cada quadro
#define Q_LIVRE   0
#define Q_SO      1     // imagem do núcleo, vetor, pilhas, ou inalcançável
#define Q_TABELA  2
#define Q_RO      3     // código (só estes podem ser substituídos)
#define Q_RW      4
#define Q_PILHA   5

// Gerenciador de quadros livres: mapa de bits dos 2048 quadros reais
// (bit 1 = ocupado). Os que ficam fora do pool são marcados ocupados para
// sempre (ver README: o núcleo, o DMA do disco e o pt só alcançam 64 KB).
int mapa_quadros[MAPA_PALAVRAS];

// informações dos quadros alcançáveis (vetores paralelos, um por campo)
int quadro_uso[QUADROS_ALC];
int quadro_dono[QUADROS_ALC];     // entrada na tabela de processos, -1 = nenhum
int quadro_pag[QUADROS_ALC];      // página virtual que ele contém
int quadro_idade[QUADROS_ALC];    // LRU: contador de envelhecimento de 8 bits
int quadro_prox[QUADROS_ALC];     // fila FIFO dos quadros de código, na ordem de carga
int quadro_ant[QUADROS_ALC];
int quadro_ent[QUADROS_ALC];      // código: endereço físico da entrada de código
                                  // (só para envelhece(), em ponte.asm)

int fifo_ini;
int fifo_fim;
int primeiro_quadro;      // primeiro quadro depois da imagem do núcleo
int n_pool;               // quadros do pool (primeiro_quadro .. FIM_POOL-1)
int limite_quadros;       // quantos o SO aceita dar aos processos
int n_usados;
int pico_usados;
int livres_inicio;
int dica_livre;           // nenhum quadro livre abaixo deste
int pagina_fixa;         // página do processo atual que não pode ser vítima (-1 = nenhuma)

int n_faltas;
int n_substituicoes;
int n_escritas;

int quadro_ocupado(int q)
{
    return (mapa_quadros[q >> 4] >> (q & 15)) & 1;
}

void marca_quadro(int q, int ocupado)
{
    int *w;
    int m;

    w = &mapa_quadros[q >> 4];
    m = 1 << (q & 15);
    if (ocupado) {
        *w = *w | m;
    } else {
        *w = *w & ~m;
    }
}

void inicia_quadros(void)
{
    int q;
    int fim;

    fim = so_fim_imagem();
    primeiro_quadro = (fim >> 9) + ((fim & 511) != 0);
    for (q = 0; q < MAPA_PALAVRAS; q++) {
        mapa_quadros[q] = -1;
    }
    for (q = 0; q < QUADROS_ALC; q++) {
        quadro_uso[q] = Q_SO;
        quadro_dono[q] = -1;
        quadro_pag[q] = -1;
        quadro_idade[q] = 0;
        quadro_prox[q] = -1;
        quadro_ant[q] = -1;
    }
    dica_livre = primeiro_quadro;
    for (q = primeiro_quadro; q < FIM_POOL; q++) {
        marca_quadro(q, 0);
        quadro_uso[q] = Q_LIVRE;
    }
    n_pool = FIM_POOL - primeiro_quadro;
    limite_quadros = n_pool;
    if (LIMITE_QUADROS > 0 && LIMITE_QUADROS < n_pool) {
        limite_quadros = LIMITE_QUADROS;
    }
    n_usados = 0;
    pico_usados = 0;
    fifo_ini = -1;
    fifo_fim = -1;
    pagina_fixa = -1;
}

int quadros_livres(void)
{
    return limite_quadros - n_usados;
}

// fila FIFO dos quadros de código, duplamente encadeada: tirar um quadro
// do meio (morte do dono, substituição) não precisa percorrer a fila
void fifo_insere(int q)
{
    quadro_prox[q] = -1;
    quadro_ant[q] = fifo_fim;
    if (fifo_fim < 0) {
        fifo_ini = q;
    } else {
        quadro_prox[fifo_fim] = q;
    }
    fifo_fim = q;
}

void fifo_remove(int q)
{
    if (quadro_ant[q] < 0) {
        fifo_ini = quadro_prox[q];
    } else {
        quadro_prox[quadro_ant[q]] = quadro_prox[q];
    }
    if (quadro_prox[q] < 0) {
        fifo_fim = quadro_ant[q];
    } else {
        quadro_ant[quadro_prox[q]] = quadro_ant[q];
    }
    quadro_prox[q] = -1;
    quadro_ant[q] = -1;
}

void libera_quadro(int q)
{
    if (quadro_uso[q] == Q_RO) {
        fifo_remove(q);
    }
    marca_quadro(q, 0);
    quadro_uso[q] = Q_LIVRE;
    quadro_dono[q] = -1;
    quadro_pag[q] = -1;
    n_usados = n_usados - 1;
    if (q < dica_livre) {
        dica_livre = q;
    }
}

// Vítima da substituição (parte III): só quadros de código, de qualquer
// processo (substituição global), menos a página fixada pela falta em
// andamento. Dados e pilha nunca saem: não voltariam por demanda.
//   FIFO: o primeiro da fila (ordem de carga).
//   LRU:  o de menor idade; no empate, o mais antigo na fila.
int escolhe_vitima(void)
{
    int q;
    int melhor;

    melhor = -1;
    q = fifo_ini;
    while (q >= 0) {
        if (!(quadro_dono[q] == atual && quadro_pag[q] == pagina_fixa)) {
#if SUBST == 1
            return q;
#else
            if (melhor < 0 || quadro_idade[q] < quadro_idade[melhor]) {
                melhor = q;
            }
#endif
        }
        q = quadro_prox[q];
    }
    return melhor;
}

// Tira a página do quadro q do processo dono: se ela foi modificada (tipo
// 3), grava de volta na área de troca do dono antes; depois marca ausentes
// as duas entradas (código e dados) NA TABELA DO DONO, que não é
// necessariamente o processo que causou a falta.
void despeja(int q)
{
    int dono;
    int pt;
    int pag;
    int e;
    int d;

    dono = quadro_dono[q];
    pt = tabela[dono].pt;
    pag = quadro_pag[q];
    e = pt_le(pt, pag);
    d = pt_le(pt, 128 + pag);
    if (TIPO(e) == 3 || TIPO(d) == 3) {
        disco_escreve(SETOR_TROCA + dono * SETORES_TROCA + pag, q);
        n_escritas = n_escritas + 1;
    }
    pt_escreve(pt, pag, 0);
    pt_escreve(pt, 128 + pag, 0);
    libera_quadro(q);
    n_substituicoes = n_substituicoes + 1;
}

// primeiro quadro livre do pool, procurando no mapa de bits a partir de
// dica_livre (abaixo dela está tudo ocupado); palavras cheias do mapa são
// puladas de 16 em 16 (os quadros fora do pool estão sempre ocupados)
int acha_livre(void)
{
    int q;
    int bits;

    q = dica_livre;
    while (q < FIM_POOL) {
        bits = mapa_quadros[q >> 4];
        if ((q & 15) == 0 && bits == -1) {
            q = q + 16;
        } else {
            if (((bits >> (q & 15)) & 1) == 0) {
                dica_livre = q + 1;
                return q;
            }
            q = q + 1;
        }
    }
    dica_livre = FIM_POOL;
    return -1;
}

// Pega um quadro para (uso, dono, pagina), respeitando limite_quadros
// (contam todos os quadros dos processos: tabelas, dados, pilha, código).
// Sem quadro livre, só com substituição (parte III) libera um de código.
// Devolve -1 se não conseguir.
int aloca_quadro(int uso, int dono, int pagina)
{
    int q;

    q = -1;
    if (n_usados < limite_quadros) {
        q = acha_livre();
    }
    if (q < 0) {
#if CARGA == 2 && SUBST != 0
        q = escolhe_vitima();
        if (q < 0) {
            return -1;
        }
        despeja(q);
#else
        return -1;
#endif
    }
    marca_quadro(q, 1);
    quadro_uso[q] = uso;
    quadro_dono[q] = dono;
    quadro_pag[q] = pagina;
    if (uso == Q_RO) {
        // a página vai ser usada agora (é a que faltou): entra como
        // acessada no último tique. Com idade 0 ela seria a próxima
        // vítima do LRU antes mesmo de o relógio a ver
        quadro_idade[q] = 128;
        quadro_ent[q] = (tabela[dono].pt << 9) + pagina * 2;
        fifo_insere(q);
    }
    n_usados = n_usados + 1;
    if (n_usados > pico_usados) {
        pico_usados = n_usados;
    }
    return q;
}

// devolve todos os quadros da entrada i (inclusive a tabela de páginas)
int libera_quadros_de(int i)
{
    int q;
    int n;

    n = 0;
    for (q = primeiro_quadro; q < FIM_POOL; q++) {
        if (quadro_dono[q] == i) {
            libera_quadro(q);
            n = n + 1;
        }
    }
    return n;
}

// carrega a página pag do programa num quadro novo e mapeia no espaço de i
// (tipo da entrada de dados: 1 = espelho só leitura do código, 3 = dados)
int carrega_pagina(int i, int pag, int uso, int tipo_dados)
{
    int q;
    int pt;

    q = aloca_quadro(uso, i, pag);
    if (q < 0) {
        return -1;
    }
    if (disco_le(prog_setor[tabela[i].prog] + pag, q) != 0) {
        k_puts("\n[SO] erro de leitura do disco\n");
        return -1;
    }
    pt = tabela[i].pt;
    if (uso == Q_RO) {
        pt_escreve(pt, pag, ENTRADA(1, q));
    }
    pt_escreve(pt, 128 + pag, ENTRADA(tipo_dados, q));
    return q;
}

// Monta o espaço de endereçamento da entrada i para o programa prog: tabela
// vazia (tudo tipo 0), dados lidos do disco, pilha zerada e, na parte I, o
// código inteiro. Na parte II as entradas de código ficam tipo 0 e a
// primeira busca de instrução (ip = 0) já falta. Se faltar memória, desfaz
// o que já alocou e devolve 0.
int monta_espaco(int i, int prog)
{
    int pt;
    int pag;
    int q;
    int n_ro;
    int n_rw;

    n_ro = prog_n_ro[prog];
    n_rw = prog_n_rw[prog];
    pt = aloca_quadro(Q_TABELA, i, -1);
    if (pt < 0) {
        return 0;
    }
    zera_quadro(pt);
    tabela[i].pt = pt;
    tabela[i].prog = prog;

    for (pag = n_ro; pag < n_ro + n_rw; pag++) {
        if (carrega_pagina(i, pag, Q_RW, 3) < 0) {
            libera_quadros_de(i);
            return 0;
        }
    }
    for (pag = PAG_PILHA; pag < 128; pag++) {
        q = aloca_quadro(Q_PILHA, i, pag);
        if (q < 0) {
            libera_quadros_de(i);
            return 0;
        }
        zera_quadro(q);
        pt_escreve(pt, 128 + pag, ENTRADA(3, q));
    }
#if CARGA == 1
    for (pag = 0; pag < n_ro; pag++) {
        if (carrega_pagina(i, pag, Q_RO, 1) < 0) {
            libera_quadros_de(i);
            return 0;
        }
    }
#endif
    return 1;
}

// ---------------------------------------------------------- processos (cont.)

int cria_processo(int prog)
{
    int i;
    int k;
    struct processo *p;
    struct registro *r;

    if (prog < 0 || prog >= N_PROGRAMAS) {
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
    if (i >= MAX_PROC) {
        return -1;
    }

    p = &tabela[i];
    if (!monta_espaco(i, prog)) {
        k_puts("\n[SO] sem memoria para criar ");
        k_puts(programa_nome(prog));
        k_putc('\n');
        return -1;
    }
    for (k = 0; k < 16; k++) {
        p->quadro[k] = 0;
    }
    // espaço virtual próprio: começa em 0 (crt0.asm); a pilha começa no
    // topo do espaço (sp = 0, o 1º push grava em 0xFFFE)
    p->quadro[Q_SP] = 0;
    p->quadro[Q_IP] = 0;
    p->quadro[Q_SR] = SR_P;        // S=0, P=1, I=0 (usuário, paginado, sem E/S), D=0
    p->quadro[Q_PT] = p->pt << 9;  // endereço físico da tabela de páginas

    p->pid = proximo_pid;
    proximo_pid = proximo_pid + 1;
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
    k_puts(programa_nome(prog));
    k_puts(") na entrada ");
    k_int(i);
    k_puts(", quadros livres ");
    k_int(quadros_livres());
    k_putc('\n');
    return p->pid;
}

void mata_processo(int i)
{
    struct processo *p;
    int k;
    int pid;
    int n;

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
    if (p->reg >= 0) {
        registros[p->reg].terminou = 1;
        registros[p->reg].termino = agora;
    }
    // devolve todos os quadros, inclusive o da tabela de páginas
    n = libera_quadros_de(i);
    k_puts("\n[SO] fim pid ");
    k_int(pid);
    k_puts(" (");
    k_puts(programa_nome(p->prog));
    k_puts("), entrada ");
    k_int(i);
    k_puts(" livre, ");
    k_int(n);
    k_puts(" quadros devolvidos\n");
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
    if (irq == 1) return "violacao de protecao";
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
    // fora do #if do escalonador: o envelhecimento roda em todo tique,
    // antes do teste de quantum
#if CARGA == 2 && SUBST == 2
    envelhece();
#endif
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

// Exceção de CPU: se veio de um processo de usuário, só ele morre (com o
// diagnóstico a partir do ip salvo; nas faltas de página, também a página
// e o motivo); se veio do próprio núcleo (ou do laço ocioso), não há como
// continuar.
void mata_com_diagnostico(int irq, int *quadro, int pag, char *motivo)
{
    k_puts("\n[SO] processo ");
    k_int(tabela[atual].pid);
    k_puts(" (");
    k_puts(programa_nome(tabela[atual].prog));
    k_puts(") morto: ");
    k_puts(nome_irq(irq));
    k_puts(" em ip=");
    k_hex(quadro[Q_IP]);
    if (pag >= 0) {
        k_puts(", pagina ");
        k_int(pag);
        k_puts(": ");
        k_puts(motivo);
    }
    k_putc('\n');
    mata_processo(atual);
}

void trata_excecao(int irq, int *quadro)
{
    if (atual >= 0 && (quadro[Q_SR] & SR_S) == 0) {
        mata_com_diagnostico(irq, quadro, -1, "");
        return;
    }
    k_puts("\n[SO] PANICO: ");
    k_puts(nome_irq(irq));
    k_puts(" no nucleo em ip=");
    k_hex(quadro[Q_IP]);
    k_putc('\n');
    para();
}

// a página pag do programa prog tem instrução que cruza a fronteira: a
// instrução começa em pag*512-2 e o imediato fica no início de pag
int cruza(int prog, int pag)
{
    int k;
    k = prog_cruza_ini[prog];
    while (prog_cruza[k] >= 0) {
        if (prog_cruza[k] == pag) {
            return 1;
        }
        k = k + 1;
    }
    return 0;
}

// Interrupção 5 (ausência de quadro). O Mancha não guarda o endereço que
// faltou; só o ip salvo (quadro[Q_IP]) é conhecido. Numa falta de busca de
// instrução ele é o endereço que faltou; numa falta de dado, é a instrução
// seguinte.
//
// Parte I (CARGA=1): tudo que o processo usa legitimamente foi mapeado na
// criação, então toda falta é erro e o processo morre.
//
// Parte II (CARGA=2): só código é carregado por demanda.
//  - Se a entrada de código da página do ip já está presente, a busca da
//    instrução teria funcionado: a falta foi de DADO, ilegítima.
//  - Se a página do ip não é de código (>= n_ro: dados, pilha ou além do
//    programa -- a comparação com as áreas de dados conhecidas do
//    processo), é salto para fora do código, ilegítimo.
//  - Senão, é falta legítima de código: aloca um quadro (talvez
//    substituindo outro, parte III), lê a página do disco (síncrono: a
//    máquina inteira espera) e mapeia como só leitura nas duas entradas.
//    O "rete" repete a busca no mesmo ip. Não há troca de processo.
//
// Instrução que cruza a fronteira de página: se a instrução começa em
// k*512-2 e o imediato está na página k ausente, a falta vem com ip = k*512
// (o ip já tinha avançado sobre a 1ª palavra) e o "rete" leria o imediato
// como instrução. O mkdisco lista essas páginas (decodificação linear do
// texto, feita na montagem); nesse caso o ip volta 2 e a página k-1 não
// pode ser a vítima: as duas precisam estar residentes juntas.
void trata_falta(int *quadro)
{
    int ip;
    int pag;
    int prog;
    int pt;
    int e;
    int q;

    if (atual < 0 || (quadro[Q_SR] & SR_S) != 0) {
        trata_excecao(5, quadro);
        return;
    }
    ip = quadro[Q_IP];
    pag = (ip >> 9) & 127;
#if CARGA == 1
    mata_com_diagnostico(5, quadro, pag, "falta de pagina com o programa todo carregado");
#else
    prog = tabela[atual].prog;
    pt = tabela[atual].pt;
    e = pt_le(pt, pag);
    if (TIPO(e) != 0) {
        mata_com_diagnostico(5, quadro, pag, "acesso a dado fora das areas do processo");
        return;
    }
    if (pag >= prog_n_ro[prog]) {
        mata_com_diagnostico(5, quadro, pag, "execucao fora do codigo do programa");
        return;
    }
    pagina_fixa = -1;
    if ((ip & 511) == 0 && pag > 0 && cruza(prog, pag)) {
        quadro[Q_IP] = ip - 2;
        pagina_fixa = pag - 1;
    }
    q = carrega_pagina(atual, pag, Q_RO, 1);
    pagina_fixa = -1;
    if (q < 0) {
        // (um quadro já alocado volta junto com os outros do processo)
        mata_com_diagnostico(5, quadro, pag, "sem quadro para a pagina de codigo");
        return;
    }
    n_faltas = n_faltas + 1;
    if (tabela[atual].reg >= 0) {
        registros[tabela[atual].reg].faltas = registros[tabela[atual].reg].faltas + 1;
    }
#endif
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

char *nome_memoria(void)
{
#if CARGA == 1
    return "carga completa (parte I)";
#else
#if SUBST == 0
    return "codigo por demanda, sem substituicao";
#endif
#if SUBST == 1
    return "codigo por demanda, substituicao FIFO";
#endif
#if SUBST == 2
    return "codigo por demanda, substituicao LRU";
#endif
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

    k_puts("\nmemoria: ");
    k_puts(nome_memoria());
    k_puts(", ");
    k_int(limite_quadros);
    k_puts(" quadros para processos (pool de ");
    k_int(n_pool);
    k_puts(" a partir do quadro ");
    k_int(primeiro_quadro);
    k_puts(")\nfaltas de pagina:  ");
    k_int(n_faltas);
    k_puts("\nsubstituicoes:     ");
    k_int(n_substituicoes);
    k_puts("\nescritas de volta: ");
    k_int(n_escritas);
    k_puts("\npico de quadros:   ");
    k_int(pico_usados);
    k_puts("\nquadros livres:    inicio ");
    k_int(livres_inicio);
    k_puts(", fim ");
    k_int(quadros_livres());

    k_puts("\n\npid programa         retorno preemp  resp.media (n) faltas\n");
    for (k = 0; k < n_registros; k++) {
        r = &registros[k];
        k_int_col(r->pid, 3);
        k_putc(' ');
        k_str_col(programa_nome(r->prog), 15);
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
        k_putc(')');
        k_int_col(r->faltas, 7 - k_digitos(r->n_respostas));
        k_putc('\n');
    }

    k_puts("\n                   vezes em estado     tempo em estado (instr)\n");
    // (em duas partes: o montador só lê linhas de até 320 caracteres, e o
    // mcc gera um .db com a string inteira numa linha)
    k_puts("pid programa        pron bloq exec");
    k_puts("     pronto  bloqueado   executando\n");
    for (k = 0; k < n_registros; k++) {
        r = &registros[k];
        k_int_col(r->pid, 3);
        k_putc(' ');
        k_str_col(programa_nome(r->prog), 15);
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
    } else if (irq == 5) {
        trata_falta(quadro);
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

    inicia_quadros();
    if (primeiro_quadro >= FIM_POOL) {
        k_puts("[SO] PANICO: nucleo sem quadros livres\n");
        para();
    }
    if (N_PROGRAMAS_DISCO != N_PROGRAMAS) {
        k_puts("[SO] PANICO: disco com N_PROGRAMAS errado\n");
        para();
    }
    if (SETOR_TROCA + MAX_PROC * SETORES_TROCA > SETORES_DISCO) {
        k_puts("[SO] PANICO: area de troca nao cabe no disco\n");
        para();
    }
    livres_inicio = quadros_livres();
    k_puts("[SO] memoria: ");
    k_puts(nome_memoria());
    k_puts("; quadros ");
    k_int(primeiro_quadro);
    k_puts("..");
    k_int(FIM_POOL - 1);
    k_puts(" livres para processos, limite ");
    k_int(limite_quadros);
    k_putc('\n');

    if (cria_processo(PROG_INIT) < 0) {
        k_puts("[SO] PANICO: nao foi possivel criar o init\n");
        para();
    }
    escalona();
    despacha(quadro);
}
