        .equ instrucoes_por_segundo = 1000
        .equ duracao = 5
        .org 0
        .dw inicio, 0xF000, 0, 0
        .org 0x80
inicio:

        ldq r0, 0
        out r0, (0x22)
        out r0, (0x23)
        out r0, (0x20)
        out r0, (0x21)
        ld r0, 255
        out r0, (0x22)
        out r0, (0x23)
        ld r4, -1
mede_laco:
        call le_relogio
        addq r0, 6
        ld r3, r0
        div r0, instrucoes_por_segundo
        cmp r0, r4
        jmpc eq, mede_laco
        ld r4, r0
        ld r0, texto_segundos
        call puts
        ld r0, r4
        call imprime_decimal
        ld r0, texto_instrucoes
        call puts
        ld r0, r3
        call imprime_decimal
        ldq r0, 10
        call putc
        cmp r4, duracao
        jmpc lt, mede_laco
        halt

le_relogio:
        in r1, (0x20)
leitura_baixo:
        in r0, (0x21)
        in r2, (0x20)
        cmp r1, r2
        jmpc ne, le_relogio
        shl r1, 8
        or r0, r1
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
texto_segundos: .db "segundos_simulados=", 0
texto_instrucoes: .db " instrucoes=", 0
