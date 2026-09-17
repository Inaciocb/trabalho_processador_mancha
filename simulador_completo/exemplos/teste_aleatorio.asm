        .equ    pilha = 0x3F0

        .org    0
        .dw     main, pilha, 0, 0

        .org    0x80
main:
        ldq     r4, 10

laco:
        call    gera_aleatorio
        call    imprime_hex_byte
        ldq     r0, 32
        call    putc

        addq    r4, -1
        cmp     r4, 0
        jmpc    nz, laco

        halt

putc:
        inb     r2, (2)
        andb    r2, 1
        jmpc    z, putc
        outb    r0, (1)
        ret

imprime_hex_byte:
        push    r3

        ld      r3, r0
        shr     r3, 4
        cmp     r3, 10
        jmpc    lt, decimal1
        addb    r3, 55
        bra     converte1
decimal1:
        addb    r3, 48
converte1:
        push    r0
        ld      r0, r3
        call    putc
        pop     r0

        ld      r3, r0
        andb    r3, 15
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

        .pub gera_aleatorio
gera_aleatorio:
        ld r1, (semente)
        ld r2, (inicializada)
        cmp r2, 0
        jmpc nz, calcula_lcg

        ld r2, 255
        out r2, (0x22)
        out r2, (0x23)
espera_semente:
        in r2, (0x02)
        and r2, 2
        jmpc z, espera_semente
        in r1, (0x21)
        in r2, (0x01)
        xor r1, r2
        ldq r2, 1
        st r2, (inicializada)
calcula_lcg:
        ldq r2, 13
        mul r1, r2
        addq r1, 7
        ldq r3, 255
        and r1, r3
        st r1, (semente)
        ld r0, r1
        ret

        .data
semente: .dw 0
inicializada: .dw 0
