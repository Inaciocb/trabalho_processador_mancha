; Montar e rodar:
;   ./bin/montador exemplos/teste_aleatorio.asm exemplos/gera_aleatorio.asm -o exemplos/teste_aleatorio.mob
;   ./bin/simulador exemplos/teste_aleatorio.mob

        .ext    gera_aleatorio

        .equ    pilha = 0x3F0

        .org    0
        .dw     main, pilha, 0, 0

        .org    0x80
main:
        ldq     r4, 10          ; quantos numeros gerar

laco:
        call    gera_aleatorio  ; r0 = proximo numero pseudo-aleatorio (0-255)
        call    imprime_hex_byte
        ldq     r0, 32          ; espaco
        call    putc

        addq    r4, -1
        cmp     r4, 0
        jmpc    nz, laco

        halt

; putc: imprime o caractere em r0 no console (espera a interface ficar
; pronta antes de enviar). Usa r2, preserva r0 e r1.

putc:
        inb     r2, (2)
        andb    r2, 1
        jmpc    z, putc
        outb    r0, (1)
        ret

; imprime_hex_byte: imprime r0 (8 bits, 0-255) em hexadecimal (2 digitos).
; Reaproveita putc; preserva r3.
imprime_hex_byte:
        push    r3

        ld      r3, r0
        shr     r3, 4                   ; nibble alto
        cmp     r3, 10
        jmpc    lt, decimal1
        addb    r3, 55                  ; 'A' - 10
        bra     converte1
decimal1:
        addb    r3, 48                  ; '0'
converte1:
        push    r0
        ld      r0, r3
        call    putc
        pop     r0

        ld      r3, r0
        andb    r3, 15                  ; nibble baixo
        cmp     r3, 10
        jmpc    lt, decimal2
        addb    r3, 55
        bra     converte2
decimal2:
        addb    r3, 48
converte2:
        push    r0
        ld      r0, r3
        call    putc
        pop     r0

        pop     r3
        ret
