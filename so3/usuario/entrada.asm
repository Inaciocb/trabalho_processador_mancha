; entrada.asm -- início de todo programa de usuário do T3.
;
; Cada programa é montado sozinho, a partir do endereço 0, com este arquivo
; PRIMEIRO na linha do montador e fim.asm por último:
;
;   montador entrada.asm runtime_so.asm biblioteca.asm prog.asm fim.asm
;
; Com paginação, todo processo começa em ip=0 virtual, que cai aqui.
;
;   endereço 0: desvio para _entrada_inicio (4 bytes: instrução + endereço)
;   endereço 4: tamanho da imagem em bytes (endereço de _fim_programa),
;               lido pelo gera_disco.sh para saber quantas páginas o
;               programa ocupa no disco

_entrada:
        jmp     _entrada_inicio
        .dw     _fim_programa

_entrada_inicio:
        call    _f_main
        jmp     _f_fim_processo         ; main voltou: o processo termina
