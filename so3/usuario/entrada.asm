; entrada.asm -- início de todo programa de usuário do T3.
;
; Cada programa é montado sozinho, a partir do endereço 0, com todo o
; código antes de todos os dados (o Makefile separa cada .asm do mcc na
; linha ".data"):
;
;   entrada runtime_so biblioteca(código) prog(código)
;   inicio_dados biblioteca(dados) prog(dados) fim
;
; Com paginação, todo processo começa em ip=0 virtual, que cai aqui.
;
;   endereço 0: desvio para _entrada_inicio (4 bytes: instrução + endereço)
;   endereço 4: tamanho da imagem em bytes (endereço de _fim_programa)
;   endereço 6: onde começam os dados (endereço de _inicio_dados)
;
; Os dois são lidos pelo gera_disco.sh.

_entrada:
        jmp     _entrada_inicio
        .dw     _fim_programa
        .dw     _inicio_dados

_entrada_inicio:
        call    _f_main
        jmp     _f_fim_processo         ; main voltou: o processo termina
