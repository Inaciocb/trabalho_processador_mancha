; ponte.asm -- o que o C do mcc não expõe: instalar os pontos de entrada do
; núcleo no vetor de interrupções e nos ganchos da BIOS, acesso a portas,
; "halt", o laço ocioso e o primeiro "rete" manual. Todo o resto do SO está
; em so.c.
;
; Duas convenções de chamada convivem aqui (ver bios_mancha/README.md):
;  - BIOS: argumentos em r0..r3 (_kernel_inicio, bios_copia_quadro);
;  - mcc: argumentos empilhados da direita para a esquerda, rótulos _f_<nome>.
;
; Todo ponto de entrada termina chamando so_trata_interrupcao(irq, quadro),
; onde "quadro" é o endereço FIXO onde a CPU empilhou as 16 palavras do
; contexto interrompido (pilha_sistema-32). A função C grava ali o contexto
; do processo escolhido, e o "rete" seguinte o retoma.

        .equ    IRQ_SEG       = 1
        .equ    IRQ_PRIV      = 2
        .equ    IRQ_DIV0      = 3
        .equ    IRQ_ILEGAL    = 4
        .equ    IRQ_PAGINA    = 5
        .equ    IRQ_SISTEMA   = 7
        .equ    IRQ_CONSOLE   = 8
        .equ    IRQ_RELOGIO   = 10

; ---------------------------------------------------------- inicialização
;
; chamado por _start (bios.asm), já em pilha_nucleo e com D=1.
_kernel_inicio:
        ld      r0, _so_entrada_relogio
        st      r0, (retomada_escalonador)

        ; tabela_syscalls[1..5] -> núcleo (as demais continuam devolvendo -1)
        ld      r0, _so_entrada_chamada
        ld      r1, tabela_syscalls
        add     r1, 2
        ld      r2, 5
_ki_laco_sc:
        st      r0, (r1)
        add     r1, 2
        add     r2, -1
        cmp     r2, 0
        jmpc    ne, _ki_laco_sc

        ; campo "ip" dos quadros 1-5 e 8 do vetor (endereço = número * 8):
        ; exceções passam a matar só o processo culpado, e a console passa a
        ; ser tratada pelo núcleo (que precisa saber da chegada de caracteres
        ; para desbloquear processos)
        ld      r0, _so_entrada_seg
        st      r0, (8)
        ld      r0, _so_entrada_priv
        st      r0, (16)
        ld      r0, _so_entrada_div0
        st      r0, (24)
        ld      r0, _so_entrada_ilegal
        st      r0, (32)
        ld      r0, _so_entrada_pagina
        st      r0, (40)
        ld      r0, _so_entrada_console
        st      r0, (64)

        ld      r0, pilha_sistema
        add     r0, -32
        push    r0
        call    _f_so_inicia
        add     sp, 2

        ; "rete" lê do sp de verdade: aponta para o quadro que so_inicia montou
        ld      sp, pilha_sistema
        add     sp, -32
        rete

; ------------------------------------------------------ pontos de entrada
;
; Chegam aqui com sp = quadro (vetor direto, ou o "jmp" do relógio da BIOS).

_so_entrada_seg:
        ld      r0, IRQ_SEG
        jmp     _so_entrada_comum
_so_entrada_priv:
        ld      r0, IRQ_PRIV
        jmp     _so_entrada_comum
_so_entrada_div0:
        ld      r0, IRQ_DIV0
        jmp     _so_entrada_comum
_so_entrada_ilegal:
        ld      r0, IRQ_ILEGAL
        jmp     _so_entrada_comum
_so_entrada_pagina:
        ld      r0, IRQ_PAGINA
        jmp     _so_entrada_comum
_so_entrada_console:
        ld      r0, IRQ_CONSOLE
        jmp     _so_entrada_comum
_so_entrada_relogio:
        ld      r0, IRQ_RELOGIO

_so_entrada_comum:
        ld      r1, sp
        push    r1
        push    r0
        call    _f_so_trata_interrupcao
        add     sp, 4
        rete

; Chamada de sistema: chega por "call" do despachante da BIOS, então o
; quadro está em sp+2 (sp aponta para o endereço de retorno). Depois do
; "ret" a BIOS grava r0 em (quadro) e faz "rete" -- por isso devolvemos em r0
; exatamente o r0 que so_trata_interrupcao deixou no quadro escolhido (que
; pode ser de outro processo, se houve troca).
_so_entrada_chamada:
        ld      r1, sp
        add     r1, 2
        push    r1
        ld      r0, IRQ_SISTEMA
        push    r0
        call    _f_so_trata_interrupcao
        add     sp, 4
        ld      r0, (sp+2)
        ret

; ------------------------------------------------------ funções para o C

; int porta_le(int porta)
_f_porta_le:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        in      r0, (r1)
        ld      sp, bp
        pop     bp
        ret

; void porta_escreve(int porta, int valor)
_f_porta_escreve:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r0, (bp+6)
        out     r0, (r1)
        ld      sp, bp
        pop     bp
        ret

; int le_relogio(void) -- contador do relógio (portas 0x20/0x21). As duas
; metades são lidas em instruções diferentes e o contador anda a cada
; instrução: relê se a parte alta mudou no meio da leitura.
_f_le_relogio:
        in      r1, (0x20)
        in      r0, (0x21)
        in      r2, (0x20)
        cmp     r1, r2
        jmpc    ne, _f_le_relogio
        shl     r1, 8
        or      r0, r1
        ret

; void copia_quadro(int *origem, int *destino) -- 16 palavras, via BIOS
_f_copia_quadro:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)
        ld      r1, (bp+6)
        call    bios_copia_quadro
        ld      sp, bp
        pop     bp
        ret

; void k_putc(int c) / void k_puts(char *s) -- impressão do núcleo pelas
; rotinas da BIOS (bem mais baratas que o mesmo laço compilado pelo mcc)
_f_k_putc:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)
        call    bios_putc
        ld      sp, bp
        pop     bp
        ret

_f_k_puts:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)
        call    bios_imprime
        ld      sp, bp
        pop     bp
        ret

; void para(void)
_f_para:
        halt
        ret

; Laço ocioso: roda em supervisor com interrupções ligadas quando nenhum
; processo está pronto. "halt" não serve para esperar (no simulador ele
; para a CPU de vez); a próxima interrupção traz o controle de volta ao
; núcleo, que descarta este contexto.
_f_so_ocioso:
        jmp     _f_so_ocioso
