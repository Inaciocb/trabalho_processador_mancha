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

        .equ    IRQ_TRAP0     = 0
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

        ; campo "ip" dos quadros 0-5 e 8 do vetor (endereço = número * 8):
        ; exceções passam a matar só o processo culpado, e a console passa a
        ; ser tratada pelo núcleo (que precisa saber da chegada de caracteres
        ; para desbloquear processos). O quadro 0 só serve para o boot, que
        ; já passou: depois dele, um "trap 0" de usuário reiniciaria o SO em
        ; modo supervisor, então passa a ser tratado como exceção.
        ld      r0, _so_entrada_trap0
        st      r0, (0)
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

_so_entrada_trap0:
        ld      r0, IRQ_TRAP0
        jmp     _so_entrada_comum
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

; ------------------------------------------------ memória física e disco
;
; O C só lida com números de quadro (0..127); a conversão quadro -> endereço
; físico (quadro * 512 = shl 9) fica aqui, porque um endereço >= 0x8000 é
; negativo no int de 16 bits do mcc. O núcleo roda com P=0: estes acessos
; são físicos.

; int pt_le(int q_tabela, int indice) -- palavra em q_tabela*512 + indice*2
_f_pt_le:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)
        shl     r0, 9
        ld      r1, (bp+6)
        add     r1, r1
        add     r0, r1
        ld      r0, (r0)
        ld      sp, bp
        pop     bp
        ret

; void pt_escreve(int q_tabela, int indice, int valor)
_f_pt_escreve:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)
        shl     r0, 9
        ld      r1, (bp+6)
        add     r1, r1
        add     r0, r1
        ld      r1, (bp+8)
        st      r1, (r0)
        ld      sp, bp
        pop     bp
        ret

; void zera_quadro(int q) -- 256 palavras, 8 por volta (um laço em C
; custaria milhares de instruções; aqui são ~360)
_f_zera_quadro:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)
        shl     r0, 9
        ld      r1, 0
        ld      r2, 32
_zq_laco:
        st      r1, (r0+)
        st      r1, (r0+)
        st      r1, (r0+)
        st      r1, (r0+)
        st      r1, (r0+)
        st      r1, (r0+)
        st      r1, (r0+)
        st      r1, (r0+)
        add     r2, -1
        cmp     r2, 0
        jmpc    ne, _zq_laco
        ld      sp, bp
        pop     bp
        ret

; int disco_le(int setor_linear, int q)
; int disco_escreve(int setor_linear, int q)
; Lê/grava um setor (512 bytes = um quadro) pelas rotinas síncronas da BIOS
; (r0=face, r1=trilha, r2=setor, r3=endereço; a máquina inteira espera).
; Setor linear s (2 faces x 40 trilhas x 18 setores, a ordem de
; dispositivos.c): face = s/720, trilha = (s/18)%40, setor = s%18.
; Devolve 0 se ok, senão o erro do controlador.
_f_disco_le:
        push    bp
        ld      bp, sp
        call    _disco_converte
        call    bios_disco_le_setor
        call    bios_disco_erro
        ld      sp, bp
        pop     bp
        ret

_f_disco_escreve:
        push    bp
        ld      bp, sp
        call    _disco_converte
        call    bios_disco_escreve_setor
        call    bios_disco_erro
        ld      sp, bp
        pop     bp
        ret

; (bp+4) = setor linear, (bp+6) = quadro -> r0..r3 da BIOS
_disco_converte:
        ld      r4, (bp+4)
        ld      r2, r4
        div     r4, 18                  ; r4 = s/18
        ld      r3, r4
        mul     r3, 18
        sub     r2, r3                  ; r2 = s%18 (setor)
        ld      r0, r4
        div     r0, 40                  ; r0 = s/720 (face)
        ld      r1, r0
        mul     r1, 40
        sub     r4, r1
        ld      r1, r4                  ; r1 = (s/18)%40 (trilha)
        ld      r3, (bp+6)
        shl     r3, 9                   ; r3 = quadro*512
        ret

; void envelhece(void) -- LRU por envelhecimento (ver so.c). Para cada
; quadro q da fila de código (fifo_ini, quadro_prox[]):
;   idade[q] = idade[q] >> 1, mais 128 se a entrada de código ou o espelho
;   de dados (256 bytes adiante na mesma tabela) tem o bit de acesso
;   (0x2000), que é então desligado. quadro_ent[q] = endereço físico da
;   entrada de código. bp é usado como rascunho (salvo na pilha).
_f_envelhece:
        push    bp
        ld      r0, (_g_fifo_ini)
_ev_laco:
        cmp     r0, 0
        jmpc    lt, _ev_fim
        add     r0, r0                  ; r0 = 2q (índice em bytes)
        ld      r3, r0
        add     r3, _g_quadro_idade     ; r3 = &quadro_idade[q]
        ld      r2, (r3)
        shr     r2, 1                   ; r2 = idade >> 1
        ld      r1, r0
        add     r1, _g_quadro_ent
        ld      r1, (r1)                ; r1 = endereço da entrada de código
        ld      r4, (r1)
        ld      bp, r4
        and     bp, 0x2000
        cmp     bp, 0
        jmpc    eq, _ev_dados
        or      r2, 128
        and     r4, 0xDFFF
        st      r4, (r1)
_ev_dados:
        add     r1, 256                 ; entrada de dados (128 entradas adiante)
        ld      r4, (r1)
        ld      bp, r4
        and     bp, 0x2000
        cmp     bp, 0
        jmpc    eq, _ev_prox
        or      r2, 128
        and     r4, 0xDFFF
        st      r4, (r1)
_ev_prox:
        st      r2, (r3)
        add     r0, _g_quadro_prox
        ld      r0, (r0)                ; q = quadro_prox[q]
        jmp     _ev_laco
_ev_fim:
        pop     bp
        ret

; int so_fim_imagem(void) -- endereço do fim da imagem do núcleo (rótulo de
; fim.asm, montado por último)
_f_so_fim_imagem:
        ld      r0, _so_fim
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
