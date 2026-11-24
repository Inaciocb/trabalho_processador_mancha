; crt0.asm -- ponto de entrada de todo programa de usuário do T3. É o
; PRIMEIRO arquivo de cada montagem, então _inicio fica no endereço virtual
; 0, onde o núcleo põe o ip inicial de todo processo. Se main retornar, o
; processo pede a própria morte (SO_MATA_PROC com pid 0); a chamada nunca
; volta, mas por segurança repete.
;
; Sem .org: o programa é montado sozinho, a partir de 0, e o mesmo binário
; serve para qualquer quadro físico (a tabela de páginas faz o resto).
; SC_MATA vem de runtime_so.asm, montado no mesmo módulo.

        .text
_inicio:
        call    _f_main
_inicio_morre:
        ld      r1, 0
        ld      r0, SC_MATA
        trap    7
        jmp     _inicio_morre
