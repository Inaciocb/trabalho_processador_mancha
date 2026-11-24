; fim.asm -- montado por ÚLTIMO (depois de bios.asm, ponte.asm e so.asm): o
; montador usa um só contador de endereço para todos os arquivos, então
; este rótulo marca o fim da imagem do núcleo (código e dados). Daqui até
; 0xD000 ficam os quadros livres para os processos (so.c, inicia_quadros).

_so_fim:
