# Makefile de nível superior: compila o simulador do Mancha completo,
# depois o compilador C, depois a BIOS de brinquedo, depois o SO do T2 e o
# do T3 -- nessa ordem, porque compilador_c usa o montador de
# simulador_completo/bin/, e bios_mancha, so e so3 usam TANTO esse montador
# QUANTO o mcc de compilador_c/bin/ (o exemplo
# bios_mancha/exemplos/multitarefa_c/ e o núcleo do SO são escritos em C,
# compilados pelo mcc).

.PHONY: all simulador_completo compilador_c bios_mancha so so3 \
        clean clean-simulador_completo clean-compilador_c clean-bios_mancha clean-so clean-so3

all: simulador_completo compilador_c bios_mancha so so3

simulador_completo:
	$(MAKE) -C simulador_completo all

compilador_c: simulador_completo
	$(MAKE) -C compilador_c all

bios_mancha: simulador_completo compilador_c
	$(MAKE) -C bios_mancha all

so: simulador_completo compilador_c
	$(MAKE) -C so all

so3: simulador_completo compilador_c
	$(MAKE) -C so3 all

clean: clean-so3 clean-so clean-compilador_c clean-bios_mancha clean-simulador_completo

clean-simulador_completo:
	$(MAKE) -C simulador_completo clean

clean-compilador_c:
	$(MAKE) -C compilador_c clean

clean-bios_mancha:
	$(MAKE) -C bios_mancha clean

clean-so:
	$(MAKE) -C so clean

clean-so3:
	$(MAKE) -C so3 clean
