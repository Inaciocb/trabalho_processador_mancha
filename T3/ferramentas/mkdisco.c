// mkdisco.c -- prepara o disco simulado do T3 a partir dos .mob dos
// programas de usuário, cada um montado sozinho (endereço virtual 0) e
// dividido em duas partes alinhadas a página:
//
//   [0, A)  texto (só instruções)          -> páginas de código, só leitura
//   [A, B)  dados (.data, strings, globais) -> páginas de dados, leitura/escrita
//
// Dois modos:
//
//   mkdisco -a passo1.mob
//       imprime ".org A", com A = fim do texto arredondado para página.
//       Usado pelo Makefile entre a 1ª e a 2ª montagem de cada programa
//       (o montador não faz aritmética em .org).
//
//   mkdisco -o disco.img -H programas_disco.h nome=arq.mob ...
//       a ordem dos argumentos define o número de cada programa (init = 0).
//       Para cada um: confere que todo byte T < A <= todo byte D, decodifica
//       o texto linearmente a partir de 0 (dado no meio do texto é erro) e
//       registra os "cruzamentos": páginas k em que a instrução que começa
//       em k*512-2 tem imediato (o imediato fica na página k; ver README).
//       Grava as imagens achatadas, uma página por setor, contíguas a
//       partir do setor 0; gera o cabeçalho com as tabelas para o núcleo;
//       e imprime um relatório com o mapa função -> página.
//
// O .mob é lido direto (formato texto, ver objeto.h) porque o carregador
// do simulador não guarda se cada byte é T ou D.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "instrucao.h"
#include "dispositivos.h"

#define TAM_PAG       512
#define MAX_PAGINAS   126     // páginas 126 e 127 são da pilha
#define TOTAL_SETORES (DISCO_FACES * DISCO_TRILHAS * DISCO_SETORES)
#define MAX_PROGS     32
#define MAX_SIMB      512

struct simb {
    char nome[64];
    unsigned endereco;
};

struct programa {
    char nome[32];
    const char *arquivo;
    unsigned char byte[65536];
    char seg[65536];               // 0, 'T' ou 'D'
    unsigned fim_texto, inicio_dados, fim_dados;
    unsigned a;                    // início dos dados (alinhado)
    int n_ro, n_rw, setor;
    int cruza[MAX_PAGINAS];
    int n_cruza;
    struct simb simb[MAX_SIMB];
    int n_simb;
};

static void falha(const char *arq, const char *msg)
{
    fprintf(stderr, "mkdisco: %s: %s\n", arq, msg);
    exit(1);
}

static void le_mob(struct programa *p)
{
    FILE *f = fopen(p->arquivo, "r");
    char linha[256];
    int tem_t = 0, tem_d = 0;

    if (f == NULL) falha(p->arquivo, "nao foi possivel abrir");
    if (fgets(linha, sizeof linha, f) == NULL || strncmp(linha, "mobj", 4) != 0)
        falha(p->arquivo, "nao e um .mob");
    p->fim_texto = 0;
    p->inicio_dados = 0x10000;
    p->fim_dados = 0;
    while (fgets(linha, sizeof linha, f) != NULL) {
        char tipo, nome[64];
        unsigned end, val;
        if (sscanf(linha, "%c %x %x", &tipo, &end, &val) == 3 && (tipo == 'T' || tipo == 'D')) {
            if (p->seg[end] != 0) falha(p->arquivo, "byte definido duas vezes");
            p->byte[end] = (unsigned char)val;
            p->seg[end] = tipo;
            if (tipo == 'T') {
                tem_t = 1;
                if (end + 1 > p->fim_texto) p->fim_texto = end + 1;
            } else {
                tem_d = 1;
                if (end < p->inicio_dados) p->inicio_dados = end;
                if (end + 1 > p->fim_dados) p->fim_dados = end + 1;
            }
        } else if (sscanf(linha, "P %63s %x", nome, &end) == 2 && p->n_simb < MAX_SIMB) {
            snprintf(p->simb[p->n_simb].nome, sizeof p->simb[0].nome, "%s", nome);
            p->simb[p->n_simb].endereco = end;
            p->n_simb++;
        }
    }
    fclose(f);
    if (!tem_t) falha(p->arquivo, "sem texto");
    if (!tem_d) p->inicio_dados = p->fim_dados = 0;
}

static unsigned alinha(unsigned v)
{
    return (v + TAM_PAG - 1) / TAM_PAG * TAM_PAG;
}

static void analisa(struct programa *p)
{
    unsigned pc, b;
    char msg[160];

    p->a = alinha(p->fim_texto);
    for (unsigned e = 0; e < 0x10000; e++) {
        if (p->seg[e] == 'T' && e >= p->a) falha(p->arquivo, "byte de texto depois de A");
        if (p->seg[e] == 'D' && e < p->a) {
            snprintf(msg, sizeof msg, "byte de dados em %04X, antes de A=%04X "
                     "(montagem sem alinha?)", e, p->a);
            falha(p->arquivo, msg);
        }
    }
    b = p->fim_dados > p->a ? p->fim_dados : p->a;
    p->n_ro = (int)(p->a / TAM_PAG);
    p->n_rw = (int)((b - p->a + TAM_PAG - 1) / TAM_PAG);
    if (p->n_ro + p->n_rw > MAX_PAGINAS) falha(p->arquivo, "programa grande demais (> 126 paginas)");

    // decodificação linear do texto: o texto tem que ser só instruções,
    // começando em 0 e terminando exatamente em fim_texto
    pc = 0;
    p->n_cruza = 0;
    while (pc < p->fim_texto) {
        instr_decod_t d;
        uint16_t palavra;
        if (pc + 1 >= p->fim_texto || p->seg[pc] != 'T' || p->seg[pc + 1] != 'T') {
            snprintf(msg, sizeof msg, "buraco ou desalinhamento no texto em %04X", pc);
            falha(p->arquivo, msg);
        }
        palavra = (uint16_t)((p->byte[pc] << 8) | p->byte[pc + 1]);
        instrucao_decodifica(palavra, &d);
        if (d.info == NULL) {
            snprintf(msg, sizeof msg, "palavra %04X em %04X nao e instrucao (dado no texto?)", palavra, pc);
            falha(p->arquivo, msg);
        }
        if (instrucao_requer_imediato(&d)) {
            if (pc + 3 >= p->fim_texto) falha(p->arquivo, "imediato alem do fim do texto");
            if ((pc + 2) % TAM_PAG == 0) p->cruza[p->n_cruza++] = (int)((pc + 2) / TAM_PAG);
            pc += 4;
        } else {
            pc += 2;
        }
    }
}

static int compara_simb(const void *x, const void *y)
{
    const struct simb *a = x, *b = y;
    return (int)a->endereco - (int)b->endereco;
}

int main(int argc, char *argv[])
{
    static struct programa progs[MAX_PROGS];
    const char *saida = NULL, *cabecalho = NULL;
    int n = 0;

    if (argc == 3 && strcmp(argv[1], "-a") == 0) {
        struct programa *p = &progs[0];
        p->arquivo = argv[2];
        le_mob(p);
        printf("; gerado por mkdisco -a: dados começam na página seguinte ao texto\n");
        printf("        .org    0x%04X\n", alinha(p->fim_texto));
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            saida = argv[++i];
        } else if (strcmp(argv[i], "-H") == 0 && i + 1 < argc) {
            cabecalho = argv[++i];
        } else {
            char *igual = strchr(argv[i], '=');
            if (igual == NULL || n >= MAX_PROGS) {
                fprintf(stderr, "mkdisco: argumento invalido: %s (use nome=arquivo.mob)\n", argv[i]);
                return 1;
            }
            snprintf(progs[n].nome, sizeof progs[n].nome, "%.*s", (int)(igual - argv[i]), argv[i]);
            progs[n].arquivo = igual + 1;
            n++;
        }
    }
    if (saida == NULL || cabecalho == NULL || n == 0) {
        fprintf(stderr, "uso: %s -a passo1.mob\n"
                        "     %s -o disco.img -H programas_disco.h nome=arq.mob ...\n", argv[0], argv[0]);
        return 1;
    }

    int setor = 0;
    for (int i = 0; i < n; i++) {
        le_mob(&progs[i]);
        analisa(&progs[i]);
        progs[i].setor = setor;
        setor += progs[i].n_ro + progs[i].n_rw;
    }
    if (setor > TOTAL_SETORES) {
        fprintf(stderr, "mkdisco: programas nao cabem no disco (%d setores)\n", setor);
        return 1;
    }

    // imagem: o disco inteiro, programas contíguos a partir do setor 0
    static unsigned char disco[TOTAL_SETORES * TAM_PAG];
    for (int i = 0; i < n; i++) {
        struct programa *p = &progs[i];
        memcpy(&disco[(size_t)p->setor * TAM_PAG], p->byte, (size_t)(p->n_ro + p->n_rw) * TAM_PAG);
    }
    FILE *f = fopen(saida, "wb");
    if (f == NULL || fwrite(disco, 1, sizeof disco, f) != sizeof disco) {
        fprintf(stderr, "mkdisco: erro ao gravar %s\n", saida);
        return 1;
    }
    fclose(f);

    // cabeçalho para o núcleo (compilado pelo mcc: só int e if)
    f = fopen(cabecalho, "w");
    if (f == NULL) {
        fprintf(stderr, "mkdisco: erro ao gravar %s\n", cabecalho);
        return 1;
    }
    fprintf(f, "// programas_disco.h -- gerado por mkdisco, nao edite.\n"
               "// Programa i: setores prog_setor[i] .. +n_ro+n_rw-1 (um por pagina);\n"
               "// cruzamentos em prog_cruza[prog_cruza_ini[i] ..] ate -1.\n\n");
    fprintf(f, "#define N_PROGRAMAS_DISCO %d\n", n);
    fprintf(f, "#define SETOR_TROCA %d\n\n", setor);
    fprintf(f, "int prog_setor[%d] = {", n);
    for (int i = 0; i < n; i++) fprintf(f, "%s%d", i ? ", " : "", progs[i].setor);
    fprintf(f, "};\nint prog_n_ro[%d] = {", n);
    for (int i = 0; i < n; i++) fprintf(f, "%s%d", i ? ", " : "", progs[i].n_ro);
    fprintf(f, "};\nint prog_n_rw[%d] = {", n);
    for (int i = 0; i < n; i++) fprintf(f, "%s%d", i ? ", " : "", progs[i].n_rw);
    int total_cruza = 0;
    fprintf(f, "};\nint prog_cruza_ini[%d] = {", n);
    for (int i = 0; i < n; i++) {
        fprintf(f, "%s%d", i ? ", " : "", total_cruza);
        total_cruza += progs[i].n_cruza + 1;
    }
    fprintf(f, "};\nint prog_cruza[%d] = {", total_cruza);
    for (int i = 0; i < n; i++) {
        for (int k = 0; k < progs[i].n_cruza; k++) fprintf(f, "%d, ", progs[i].cruza[k]);
        fprintf(f, "-1%s", i + 1 < n ? ", " : "");
    }
    fprintf(f, "};\n\nchar *programa_nome(int prog)\n{\n");
    for (int i = 0; i < n; i++) fprintf(f, "    if (prog == %d) return \"%s\";\n", i, progs[i].nome);
    fprintf(f, "    return \"?\";\n}\n");
    fclose(f);

    // relatório
    printf("prog nome            setor n_ro n_rw  texto   A      fim_dados  cruzamentos\n");
    for (int i = 0; i < n; i++) {
        struct programa *p = &progs[i];
        printf("%4d %-15s %5d %4d %4d  %04X    %04X   %04X      ", i, p->nome, p->setor,
               p->n_ro, p->n_rw, p->fim_texto, p->a, p->fim_dados);
        if (p->n_cruza == 0) printf("-");
        for (int k = 0; k < p->n_cruza; k++) printf("%s%d", k ? "," : "", p->cruza[k]);
        printf("\n");
    }
    printf("setores usados: %d de %d (area de troca a partir do setor %d)\n", setor, TOTAL_SETORES, setor);
    for (int i = 0; i < n; i++) {
        struct programa *p = &progs[i];
        if (p->n_simb == 0) continue;
        qsort(p->simb, (size_t)p->n_simb, sizeof p->simb[0], compara_simb);
        printf("\n%s: funcao -> pagina\n", p->nome);
        for (int k = 0; k < p->n_simb; k++)
            printf("  %04X  pag %3u  %s\n", p->simb[k].endereco, p->simb[k].endereco / TAM_PAG, p->simb[k].nome);
    }
    return 0;
}
