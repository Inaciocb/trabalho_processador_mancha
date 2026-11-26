# Makefile do mini SO do T3 (partiu do SO do T2).
#
#   make                         monta build/so.mob com a configuração padrão
#                                e o disco com os programas de usuário
#   make ESCALONADOR=3 QUANTUM=2 escolhe outra configuração
#   make roda                    roda sem interface (ferramentas/executor)
#   make relatorio               roda as configurações do relatório
#   make disco                   só o disco: build/usuario/disco.img e
#                                build/usuario/programas_disco.h
#   make confere-disco           confere o disco contra as imagens
#   make testes                  testes nativos e autoteste de memória no boot
#   make protecao                cenário de proteção de memória (PRIMEIRO=protecao)
#
# ESCALONADOR: 1 = simples (parte I), 2 = round-robin, 3 = prioridade
# QUANTUM:     em interrupções de relógio (até 32)
# INTERVALO:   instruções entre interrupções de relógio (até 30000); precisa
#              ser maior que o maior trecho do núcleo (ver README)
# BLOQUEIO:    1 = E/S bloqueia o processo, 0 = devolve erro (espera ocupada)
# AUTOTESTE_MEM: 1 = testa o gerenciador de quadros no boot
# PRIMEIRO:    programa do primeiro processo (init = cenário do T2)

ESCALONADOR ?= 2
QUANTUM     ?= 4
INTERVALO   ?= 5000
BLOQUEIO    ?= 1
AUTOTESTE_MEM ?= 0
PRIMEIRO    ?= init

# entrada simulada para o processo "eco" (instante:texto)
ENTRADA ?= -e 300000:ola -e 900000:mundo -e 2000000:fim

RAIZ     = ..
MCC      = $(RAIZ)/compilador_c/bin/mcc
MONTADOR = $(RAIZ)/simulador_completo/bin/montador
BIOS     = $(RAIZ)/bios_mancha/src/bios.asm
BIBLIO   = $(RAIZ)/compilador_c/rt/biblioteca.asm
SIM_SRC  = $(RAIZ)/simulador_completo/src
MOB2BIN  = $(RAIZ)/simulador_completo/bin/mob2bin
EXECUTOR = bin/executor

# cada configuração tem seu diretório: trocar de configuração nunca reusa
# um núcleo compilado com outra
CFG = build/e$(ESCALONADOR)-q$(QUANTUM)-i$(INTERVALO)-b$(BLOQUEIO)$(if $(filter 1,$(AUTOTESTE_MEM)),-m)$(if $(filter-out init,$(PRIMEIRO)),-$(PRIMEIRO))

# programas de usuário, na ordem dos PROG_* de src/so_chamadas.h (o
# gera_disco.sh confere)
PROGRAMAS = init calc eco misto eterno invasor ola protecao vetor autocod salto pilha gemeo
UDIR      = build/usuario
DISCO     = $(UDIR)/disco.img

.PHONY: all roda protecao relatorio disco confere-disco testes clean FORCE

all: build/so.mob $(EXECUTOR) disco

# atalho para a configuração pedida nesta chamada (recopiado sempre: a data
# não diz de qual configuração ele veio)
build/so.mob: $(CFG)/so.mob FORCE
	@cp $< $@

$(CFG)/config.h:
	@mkdir -p $(CFG)
	printf '#define ESCALONADOR %s\n#define QUANTUM %s\n#define INTERVALO %s\n#define BLOQUEIO %s\n#define AUTOTESTE_MEM %s\n#define PRIMEIRO PROG_%s\n' \
	  $(ESCALONADOR) $(QUANTUM) $(INTERVALO) $(BLOQUEIO) $(AUTOTESTE_MEM) $(shell echo $(PRIMEIRO) | tr a-z A-Z) > $@

$(CFG)/so.asm: src/so.c src/so_chamadas.h src/mem.h $(CFG)/config.h $(UDIR)/programas_disco.h
	$(MCC) -I$(CFG) -Isrc -I$(UDIR) src/so.c -o $@

build/mem.asm: src/mem.c src/mem.h
	@mkdir -p build
	$(MCC) -Isrc src/mem.c -o $@

# fim_nucleo.asm por último: marca onde começam os quadros livres
NUCLEO = $(BIOS) src/ponte.asm $(CFG)/so.asm build/mem.asm src/fim_nucleo.asm

$(CFG)/so.mob: $(NUCLEO)
	$(MONTADOR) $(NUCLEO) -o $@

# ---- programas de usuário: cada um compilado e montado sozinho, a partir
# do endereço 0, com o código antes dos dados (ver usuario/entrada.asm)

$(UDIR)/%.asm: usuario/%.c usuario/lib_usuario.c usuario/so.h src/so_chamadas.h
	@mkdir -p $(UDIR)
	$(MCC) -I$(RAIZ)/compilador_c/inc -Isrc -Iusuario $< -o $@

$(UDIR)/biblioteca.asm: $(BIBLIO)
	@mkdir -p $(UDIR)
	cp $< $@

# o mcc põe os dados depois de uma linha ".data": separa código e dados
$(UDIR)/%.codigo.asm: $(UDIR)/%.asm
	sed '/^\.data/,$$d' $< > $@

$(UDIR)/%.dados.asm: $(UDIR)/%.asm
	sed -n '/^\.data/,$$p' $< > $@

PARTES_CODIGO = usuario/entrada.asm usuario/runtime_so.asm $(UDIR)/biblioteca.codigo.asm $(UDIR)/%.codigo.asm
PARTES_DADOS  = $(UDIR)/biblioteca.dados.asm $(UDIR)/%.dados.asm usuario/fim.asm

# duas montagens: a primeira só descobre onde o código termina, para a
# segunda começar os dados numa página nova (código e dados nunca dividem
# uma página)
$(UDIR)/%.pre.mob: $(PARTES_CODIGO) usuario/inicio_dados.asm $(PARTES_DADOS)
	$(MONTADOR) $^ -o $@

$(UDIR)/%.pre.full: $(UDIR)/%.pre.mob
	$(MOB2BIN) $< 65536 $@

$(UDIR)/%.alinha.asm: $(UDIR)/%.pre.full
	od -An -tu1 -j6 -N2 $< | awk 'NF { a = $$1 * 256 + $$2; printf ".org %d\n_inicio_dados:\n", int((a + 511) / 512) * 512; exit }' > $@

$(UDIR)/%.mob: $(PARTES_CODIGO) $(UDIR)/%.alinha.asm $(PARTES_DADOS)
	$(MONTADOR) $^ -o $@

# imagem crua de 64 KB; o gera_disco.sh corta no tamanho que a capa informa
$(UDIR)/%.full: $(UDIR)/%.mob
	$(MOB2BIN) $< 65536 $@

$(DISCO): ferramentas/gera_disco.sh src/so_chamadas.h $(PROGRAMAS:%=$(UDIR)/%.full)
	sh ferramentas/gera_disco.sh src/so_chamadas.h $(UDIR) $(PROGRAMAS)

$(UDIR)/programas_disco.h: $(DISCO)

disco: $(DISCO)

confere-disco: $(DISCO)
	sh ferramentas/confere_disco.sh $(UDIR) $(PROGRAMAS)

# os .asm/.mob/.full intermediários ficam (úteis para depurar)
.SECONDARY:

$(EXECUTOR): ferramentas/executor.c $(SIM_SRC)/cpu.c $(SIM_SRC)/dispositivos.c $(SIM_SRC)/memoria.c $(SIM_SRC)/objeto.c $(SIM_SRC)/instrucao.c
	@mkdir -p bin
	$(CC) -Wall -Wextra -std=gnu11 -O2 -I$(SIM_SRC) -o $@ $^

roda: all
	$(EXECUTOR) -D $(DISCO) $(CFG)/so.mob $(ENTRADA)

testes: $(EXECUTOR) disco
	@mkdir -p bin
	$(CC) -Wall -Wextra -Wno-int-to-pointer-cast -std=gnu11 -o bin/teste_mem tests/mem.c
	./bin/teste_mem
	$(CC) -std=gnu11 -w -Itests -I$(UDIR) -o bin/teste_nucleo tests/nucleo.c
	./bin/teste_nucleo
	$(MAKE) AUTOTESTE_MEM=1 build/so.mob
	$(EXECUTOR) -D $(DISCO) build/so.mob $(ENTRADA) | grep -E '\[SO\] (memoria|autoteste|PANICO)'

protecao:
	$(MAKE) roda PRIMEIRO=protecao

relatorio: $(EXECUTOR) disco
	./relatorio.sh

clean:
	rm -rf build bin
