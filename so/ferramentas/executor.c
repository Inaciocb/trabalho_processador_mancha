// executor.c -- roda um .mob no simulador do Mancha completo sem a
// interface curses: a saída da console vai para stdout, e a entrada pode
// ser agendada para instantes fixos (simulando um operador digitando). Usa
// os mesmos módulos do simulador (cpu, dispositivos, memória, objeto), só
// troca o laço interativo de main.c.
//
// uso: executor arquivo.mob [-n max_instrucoes] [-e instrucao:texto]...
//
// Ao terminar, informa em stderr quantas instruções rodaram e o maior
// trecho contínuo executado no núcleo com interrupções desligadas -- o
// intervalo do relógio precisa ser maior que isso (ver README).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memoria.h"
#include "dispositivos.h"
#include "cpu.h"
#include "objeto.h"

#define MAX_ENTRADAS 32

struct entrada {
    long instante;
    const char *texto;
    int feita;
};

int main(int argc, char *argv[])
{
    const char *arquivo = NULL;
    long maximo = 20000000;
    struct entrada entradas[MAX_ENTRADAS];
    int n_entradas = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            maximo = atol(argv[++i]);
        } else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
            char *arg = argv[++i];
            char *dois_pontos = strchr(arg, ':');
            if (dois_pontos == NULL || n_entradas >= MAX_ENTRADAS) {
                fprintf(stderr, "entrada invalida: %s (use instrucao:texto)\n", arg);
                return 1;
            }
            *dois_pontos = '\0';
            entradas[n_entradas].instante = atol(arg);
            entradas[n_entradas].texto = dois_pontos + 1;
            entradas[n_entradas].feita = 0;
            n_entradas++;
        } else if (arquivo == NULL) {
            arquivo = argv[i];
        }
    }
    if (arquivo == NULL) {
        fprintf(stderr, "uso: %s arquivo.mob [-n max_instrucoes] [-e instrucao:texto]...\n", argv[0]);
        return 1;
    }

    mem_t *mem = mem_cria();
    disp_t *disp = disp_cria(mem);
    cpu_t *cpu = cpu_cria(mem, disp);

    char erro[256];
    simbolo_t *simbolos = NULL;
    if (!obj_carrega(arquivo, mem, &simbolos, erro, sizeof(erro))) {
        fprintf(stderr, "erro ao carregar '%s': %s\n", arquivo, erro);
        return 1;
    }
    obj_libera_simbolos(simbolos);
    cpu_liga(cpu);

    long trecho = 0, maior_trecho = 0, trecho_inicial = -1;
    long n_nucleo = 0, n_ocioso = 0, n_usuario = 0;
    while (!cpu_parada(cpu) && cpu_num_instrucoes(cpu) < maximo) {
        for (int i = 0; i < n_entradas; i++) {
            if (!entradas[i].feita && cpu_num_instrucoes(cpu) >= entradas[i].instante) {
                for (const char *p = entradas[i].texto; *p != '\0'; p++) console_poe_entrada(disp, *p);
                console_poe_entrada(disp, '\n');
                entradas[i].feita = 1;
            }
        }

        cpu_executa_1(cpu);

        int tam;
        const char *saida = console_saida(disp, &tam);
        if (tam > 0) {
            fwrite(saida, 1, (size_t)tam, stdout);
            console_limpa_saida(disp);
        }

        // classifica a instrução que vai executar a seguir: núcleo (S=1 com
        // interrupções desligadas), laço ocioso (S=1, D=0) ou usuário (S=0)
        if (cpu_modo_supervisor(cpu) && cpu_bit(cpu, SR_D)) {
            trecho++;
            n_nucleo++;
        } else {
            if (trecho > 0) {
                if (trecho_inicial < 0) trecho_inicial = trecho;
                else if (trecho > maior_trecho) maior_trecho = trecho;
            }
            trecho = 0;
            if (cpu_modo_supervisor(cpu)) n_ocioso++;
            else n_usuario++;
        }
    }
    fflush(stdout);

    int parou = cpu_parada(cpu);
    fprintf(stderr, "[executor] %s apos %ld instrucoes\n",
            parou ? "halt" : "limite atingido", cpu_num_instrucoes(cpu));
    fprintf(stderr, "[executor] nucleo %ld, ocioso %ld, usuario %ld instrucoes\n",
            n_nucleo, n_ocioso, n_usuario);
    fprintf(stderr, "[executor] maior trecho continuo no nucleo: %ld instrucoes (inicializacao: %ld)\n",
            maior_trecho, trecho_inicial);

    cpu_destroi(cpu);
    disp_destroi(disp);
    mem_destroi(mem);
    return parou ? 0 : 2;
}
