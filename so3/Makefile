# Makefile do mini SO do T2.
#
#   make                         monta build/so.mob com a configuração padrão
#   make ESCALONADOR=3 QUANTUM=2 escolhe outra configuração
#   make roda                    roda sem interface (ferramentas/executor)
#   make relatorio               roda as configurações do relatório
#
# ESCALONADOR: 1 = simples (parte I), 2 = round-robin, 3 = prioridade
# QUANTUM:     em interrupções de relógio (até 32)
# INTERVALO:   instruções entre interrupções de relógio (até 30000); precisa
#              ser maior que o maior trecho do núcleo (ver README)
# BLOQUEIO:    1 = E/S bloqueia o processo, 0 = devolve erro (espera ocupada)

ESCALONADOR ?= 2
QUANTUM     ?= 4
INTERVALO   ?= 5000
BLOQUEIO    ?= 1

# entrada simulada para o processo "eco" (instante:texto)
ENTRADA ?= -e 300000:ola -e 900000:mundo -e 2000000:fim

RAIZ     = ..
MCC      = $(RAIZ)/compilador_c/bin/mcc
MONTADOR = $(RAIZ)/simulador_completo/bin/montador
BIOS     = $(RAIZ)/bios_mancha/src/bios.asm
BIBLIO   = $(RAIZ)/compilador_c/rt/biblioteca.asm
SIM_SRC  = $(RAIZ)/simulador_completo/src
EXECUTOR = bin/executor

# cada configuração tem seu diretório: trocar de configuração nunca reusa
# um núcleo compilado com outra
CFG = build/e$(ESCALONADOR)-q$(QUANTUM)-i$(INTERVALO)-b$(BLOQUEIO)

.PHONY: all roda relatorio clean FORCE

all: build/so.mob $(EXECUTOR)

# atalho para a configuração pedida nesta chamada (recopiado sempre: a data
# não diz de qual configuração ele veio)
build/so.mob: $(CFG)/so.mob FORCE
	@cp $< $@

$(CFG)/config.h:
	@mkdir -p $(CFG)
	printf '#define ESCALONADOR %s\n#define QUANTUM %s\n#define INTERVALO %s\n#define BLOQUEIO %s\n' \
	  $(ESCALONADOR) $(QUANTUM) $(INTERVALO) $(BLOQUEIO) > $@

$(CFG)/so.asm: src/so.c src/so_chamadas.h $(CFG)/config.h
	$(MCC) -I$(CFG) -Isrc src/so.c -o $@

build/programas.asm: usuario/programas.c usuario/so.h src/so_chamadas.h
	@mkdir -p build
	$(MCC) -I$(RAIZ)/compilador_c/inc -Isrc -Iusuario usuario/programas.c -o $@

$(CFG)/so.mob: $(BIOS) src/ponte.asm $(CFG)/so.asm usuario/runtime_so.asm $(BIBLIO) build/programas.asm
	$(MONTADOR) $(BIOS) src/ponte.asm $(CFG)/so.asm usuario/runtime_so.asm $(BIBLIO) build/programas.asm -o $@

$(EXECUTOR): ferramentas/executor.c $(SIM_SRC)/cpu.c $(SIM_SRC)/dispositivos.c $(SIM_SRC)/memoria.c $(SIM_SRC)/objeto.c $(SIM_SRC)/instrucao.c
	@mkdir -p bin
	$(CC) -Wall -Wextra -std=gnu11 -O2 -I$(SIM_SRC) -o $@ $^

roda: all
	$(EXECUTOR) $(CFG)/so.mob $(ENTRADA)

relatorio: $(EXECUTOR)
	./relatorio.sh

clean:
	rm -rf build bin
