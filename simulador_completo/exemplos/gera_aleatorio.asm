.data

semente: .dw 0

.text

gera_aleatorio:

    ld r1, (semente)
    cmp r1, 0
    jmpc nz, calcula_lcg
    in r1, (0020)

calcula_lcg:

    ldq r2, 13
    mul r1, r2
    addq r1, 7
    ldq r3, 255
    and r1, r3
    st r1, (semente)
    ld r0, r1
    ret
