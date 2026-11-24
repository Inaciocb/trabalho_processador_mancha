# Makefile do mini SO do T3 (paginação e processos de usuário).
#
#   make                          monta o núcleo (build/so.mob), os programas
#                                 e o disco (build/disco.img) na configuração
#                                 padrão
#   make CARGA=1 ...              escolhe outra configuração
#   make roda                     roda sem interface (ferramentas/executor),
#                                 sobre uma cópia limpa do disco
#   make relatorio                roda as configurações do relatório
#   make testes                   testes nativos (gcc) do núcleo
#
# Do T2 (mesmo significado):
#   ESCALONADOR: 1 = simples, 2 = round-robin, 3 = prioridade
#   QUANTUM:     em interrupções de relógio (até 32)
#   INTERVALO:   instruções entre interrupções de relógio; precisa ser maior
#                que o maior trecho do núcleo (ver README)
#   BLOQUEIO:    1 = E/S bloqueia o processo, 0 = devolve erro
# Novos no T3:
#   CARGA:          1 = carga completa na criação (parte I),
#                   2 = código por demanda (parte II)
#   SUBST:          substituição quando faltam quadros (parte III, só com
#                   CARGA=2): 0 = nenhuma, 1 = FIFO, 2 = LRU (envelhecimento)
#   LIMITE_QUADROS: máximo de quadros para processos de usuário (0 = todos
#                   os quadros livres alcançáveis)

ESCALONADOR    ?= 2
QUANTUM        ?= 4
INTERVALO      ?= 10000
BLOQUEIO       ?= 1
CARGA          ?= 2
SUBST          ?= 2
LIMITE_QUADROS ?= 0

# entrada simulada para o processo "eco" (instante:texto). Chega logo no
# início, para o tempo total não ficar preso ao instante da última linha
# (ver README, relatório).
ENTRADA ?= -e 1000:ola -e 1000:mundo -e 1000:fim

RAIZ     = ..
MCC      = $(RAIZ)/compilador_c/bin/mcc
MONTADOR = $(RAIZ)/simulador_completo/bin/montador
BIOS     = $(RAIZ)/bios_mancha/src/bios.asm
BIBLIO   = $(RAIZ)/compilador_c/rt/biblioteca.asm
INC_MCC  = $(RAIZ)/compilador_c/inc
SIM_SRC  = $(RAIZ)/simulador_completo/src
EXECUTOR = bin/executor
MKDISCO  = bin/mkdisco

# Programas no disco, NA ORDEM dos números PROG_* de src/so_chamadas.h.
PROGRAMAS = init calc eco misto eterno invasor ola paginas \
            escreve_codigo le_longe salta_longe

# cada configuração tem seu diretório: trocar de configuração nunca reusa
# um núcleo compilado com outra. Os programas e o disco não dependem da
# configuração e ficam em build/u e build/.
CFG = build/e$(ESCALONADOR)-q$(QUANTUM)-i$(INTERVALO)-b$(BLOQUEIO)-c$(CARGA)-s$(SUBST)-l$(LIMITE_QUADROS)
U   = build/u

MOBS = $(PROGRAMAS:%=$(U)/%.mob)

.PHONY: all roda relatorio testes clean FORCE
.SECONDARY:

all: build/so.mob build/disco.img $(EXECUTOR)

# atalho para a configuração pedida nesta chamada (recopiado sempre: a data
# não diz de qual configuração ele veio)
build/so.mob: $(CFG)/so.mob FORCE
	@cp $< $@

# ------------------------------------------------------------ ferramentas

$(EXECUTOR): ferramentas/executor.c $(SIM_SRC)/cpu.c $(SIM_SRC)/dispositivos.c $(SIM_SRC)/memoria.c $(SIM_SRC)/objeto.c $(SIM_SRC)/instrucao.c
	@mkdir -p bin
	$(CC) -Wall -Wextra -std=gnu11 -O2 -I$(SIM_SRC) -o $@ $^

$(MKDISCO): ferramentas/mkdisco.c $(SIM_SRC)/instrucao.c
	@mkdir -p bin
	$(CC) -Wall -Wextra -std=gnu11 -O2 -I$(SIM_SRC) -o $@ $^

# --------------------------------------------------- programas de usuário
#
# Cada programa é montado SOZINHO, a partir do endereço virtual 0, com
# crt0.asm primeiro (entrada em 0). O mcc gera texto e dados no mesmo
# arquivo; separamos os dois (.texto/.dados) para montar todo o texto
# antes e todos os dados depois, começando na página seguinte ao fim do
# texto. Como o montador não faz aritmética em .org, a montagem é em dois
# passos: o 1º (.m1) só mede o fim do texto; mkdisco -a gera o .org
# alinhado (.alinha); o 2º monta de verdade. O .pub torna públicos os
# rótulos de função, para o mapa função -> página do mkdisco.

$(U)/%.asm: usuario/%.c usuario/so.h src/so_chamadas.h
	@mkdir -p $(U)
	$(MCC) -I$(INC_MCC) -Isrc -Iusuario $< -o $@

$(U)/biblioteca.asm: $(BIBLIO)
	@mkdir -p $(U)
	cp $< $@

$(U)/%.texto: $(U)/%.asm
	@: > $(U)/$*.texto
	@: > $(U)/$*.dados
	awk -v t=$(U)/$*.texto -v d=$(U)/$*.dados \
	  'BEGIN { saida = t } /^[ \t]*\.text/ { saida = t } /^[ \t]*\.data/ { saida = d } { print > saida }' $<

$(U)/%.dados: $(U)/%.texto ;

TEXTO_COMUM = $(U)/ulib.texto usuario/runtime_so.asm $(U)/biblioteca.texto
DADOS_COMUNS = $(U)/ulib.dados $(U)/biblioteca.dados

$(U)/%.pub: $(U)/%.texto $(TEXTO_COMUM)
	cat $^ | sed -n 's/^\(_f_[A-Za-z0-9_]*\):.*/        .pub    \1/p' > $@

$(U)/%.m1: usuario/crt0.asm $(U)/%.texto $(TEXTO_COMUM) $(U)/%.dados $(DADOS_COMUNS)
	$(MONTADOR) usuario/crt0.asm $(U)/$*.texto $(TEXTO_COMUM) $(U)/$*.dados $(DADOS_COMUNS) -o $@ > /dev/null

$(U)/%.alinha: $(U)/%.m1 $(MKDISCO)
	$(MKDISCO) -a $< > $@

$(U)/%.mob: usuario/crt0.asm $(U)/%.texto $(TEXTO_COMUM) $(U)/%.alinha $(U)/%.dados $(DADOS_COMUNS) $(U)/%.pub
	$(MONTADOR) usuario/crt0.asm $(U)/$*.texto $(TEXTO_COMUM) $(U)/$*.alinha \
	  $(U)/$*.dados $(DADOS_COMUNS) $(U)/$*.pub -o $@ > /dev/null

# disco: os programas, um setor por página, contíguos a partir do setor 0;
# as tabelas para o núcleo em programas_disco.h; relatório em mkdisco.txt
build/disco.img: $(MOBS) $(MKDISCO)
	$(MKDISCO) -o $@ -H build/programas_disco.h $(foreach p,$(PROGRAMAS),$(p)=$(U)/$(p).mob) > build/mkdisco.txt
	@cat build/mkdisco.txt | sed -n '1,/^setores usados/p'

build/programas_disco.h: build/disco.img ;

# ------------------------------------------------------------------ núcleo

$(CFG)/config.h:
	@mkdir -p $(CFG)
	printf '#define ESCALONADOR %s\n#define QUANTUM %s\n#define INTERVALO %s\n#define BLOQUEIO %s\n#define CARGA %s\n#define SUBST %s\n#define LIMITE_QUADROS %s\n' \
	  $(ESCALONADOR) $(QUANTUM) $(INTERVALO) $(BLOQUEIO) $(CARGA) $(SUBST) $(LIMITE_QUADROS) > $@

$(CFG)/so.asm: src/so.c src/so_chamadas.h $(CFG)/config.h build/programas_disco.h
	$(MCC) -I$(CFG) -Ibuild -Isrc src/so.c -o $@

# fim.asm por último: o rótulo dele marca o fim da imagem do núcleo
$(CFG)/so.mob: $(BIOS) src/ponte.asm $(CFG)/so.asm src/fim.asm
	$(MONTADOR) $(BIOS) src/ponte.asm $(CFG)/so.asm src/fim.asm -o $@

# ------------------------------------------------------------------ execução

# o simulador regrava a imagem do disco ao sair: cada execução usa uma
# cópia limpa, e build/disco.img nunca muda
roda: all
	cp build/disco.img $(CFG)/disco.img
	$(EXECUTOR) $(CFG)/so.mob -D $(CFG)/disco.img $(ENTRADA)

relatorio: $(EXECUTOR)
	./relatorio.sh

# testes nativos do núcleo (gcc), com portas, ponte, memória física e disco
# simulados (tests/programas_disco.h é um disco falso)
testes:
	@mkdir -p bin
	for s in 1 2; do \
	  $(CC) -std=gnu11 -Wall -Wno-pointer-to-int-cast -Wno-unused-function -DSUBST=$$s -Itests -o bin/testes_s$$s tests/nucleo.c && ./bin/testes_s$$s || exit 1; \
	done

clean:
	rm -rf build bin
