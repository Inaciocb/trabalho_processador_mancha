        .org 0
        .dw inicio, 0xF000, 0, 0
        .org 0x80
inicio:
        ld r0, mensagem
        call puts
        call gera_aleatorio
        call imprime_decimal
        push r0
        ldq r0, 10
        call putc
        pop r0
        halt

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

putc:
        push r1
putc_espera:
        in r1, (0x02)
        and r1, 1
        jmpc z, putc_espera
        outb r0, (0x01)
        pop r1
        ret

puts:
        push r0
        push r1
        ld r1, r0
puts_laco:
        ldq r0, 0
        ldb r0, (r1+)
        cmp r0, 0
        jmpc z, puts_fim
        call putc
        jmp puts_laco
puts_fim:
        pop r1
        pop r0
        ret

imprime_decimal:
        push r0
        push r1
        push r2
        ld r1, r0
        div r0, 10
        ld r2, r0
        mul r2, 10
        sub r1, r2
        cmp r0, 0
        jmpc z, imprime_digito
        call imprime_decimal
imprime_digito:
        ld r0, r1
        addq r0, 48
        call putc
        pop r2
        pop r1
        pop r0
        ret

        .data
semente: .dw 0
inicializada: .dw 0
mensagem: .db "Envie E + Enter para gerar: ", 0
