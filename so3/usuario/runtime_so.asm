; runtime_so.asm -- runtime dos programas de usuário: adaptação de
; compilador_c/rt/runtime.asm para rodar sobre o SO. Em vez de in/out/halt
; (proibidos em modo usuário), tudo vira chamada de sistema ("trap 7").
; Sem _start nem quadro de boot: quem dá a partida é a BIOS + o núcleo.
;
; Convenção do mcc: argumentos em (bp+4), (bp+6)...; resultado em r0.
; Números das chamadas: mesmos de src/so_chamadas.h.

        .equ    SC_LE      = 1
        .equ    SC_ESCREVE = 2
        .equ    SC_CRIA    = 3
        .equ    SC_MATA    = 4
        .equ    SC_ESPERA  = 5

; ----------------------------------------------------- chamadas de sistema

; int so_le(void)
_f_so_le:
        ld      r0, SC_LE
        trap    7
        ret

; int so_escreve(int c)
_f_so_escreve:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r0, SC_ESCREVE
        trap    7
        ld      sp, bp
        pop     bp
        ret

; int so_cria_proc(int prog)
_f_so_cria_proc:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r0, SC_CRIA
        trap    7
        ld      sp, bp
        pop     bp
        ret

; int so_mata_proc(int pid)
_f_so_mata_proc:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r0, SC_MATA
        trap    7
        ld      sp, bp
        pop     bp
        ret

; int so_espera_proc(int pid)
_f_so_espera_proc:
        push    bp
        ld      bp, sp
        ld      r1, (bp+4)
        ld      r0, SC_ESPERA
        trap    7
        ld      sp, bp
        pop     bp
        ret

; ------------------------------------------- mancha.h sobre o SO

; void mancha_out(int porta, int valor) -- só a porta de dados da console
; (1) faz sentido para um processo. Com o SO sem bloqueio (BLOQUEIO=0) a
; chamada pode devolver -1: tenta de novo (espera ocupada, como na parte I).
_f_mancha_out:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)
        cmp     r0, 1
        jmpc    ne, _rso_mo_fim
_rso_mo_tenta:
        ld      r1, (bp+6)
        ld      r0, SC_ESCREVE
        trap    7
        cmp     r0, 0
        jmpc    lt, _rso_mo_tenta
_rso_mo_fim:
        ld      sp, bp
        pop     bp
        ret

; int mancha_in(int porta) -- porta 2 (estado) responde sempre "há
; caractere", porque quem espera de verdade é o SO dentro de SO_LE; porta 1
; (dados) faz a chamada.
_f_mancha_in:
        push    bp
        ld      bp, sp
        ld      r0, (bp+4)
        cmp     r0, 2
        jmpc    ne, _rso_mi_dados
        ld      r0, 3
        jmp     _rso_mi_fim
_rso_mi_dados:
        cmp     r0, 1
        jmpc    ne, _rso_mi_zero
_rso_mi_tenta:
        ld      r0, SC_LE
        trap    7
        cmp     r0, 0
        jmpc    lt, _rso_mi_tenta
        jmp     _rso_mi_fim
_rso_mi_zero:
        ld      r0, 0
_rso_mi_fim:
        ld      sp, bp
        pop     bp
        ret

; void mancha_halt(void) -- para um processo, "parar" é morrer
_f_mancha_halt:
        ld      r0, SC_MATA
        ld      r1, 0
        trap    7
        ret

; ---------------------------------------------------------- utilitários

; Endereço de retorno colocado pelo SO no topo da pilha de todo processo:
; se a função do programa retornar, o processo termina aqui.
_f_fim_processo:
        ld      r0, SC_MATA
        ld      r1, 0
        trap    7
        jmp     _f_fim_processo

; void tenta_es(void)
_f_tenta_es:
        ld      r0, 88
        out     r0, (1)
        ret
