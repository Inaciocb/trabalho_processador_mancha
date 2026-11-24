# T3 — Paginação e processos de usuário

SO do T2 com processos isolados por paginação, programas carregados do disco
simulado, código por demanda e substituição FIFO/LRU.

**Requer o simulador deste repositório** (`cpu_interrompe` desliga o bit P ao
entrar numa interrupção; sem isso um processo paginado interrompido trava).

## Uso

```sh
make                      # núcleo, programas e build/disco.img
make CARGA=1 roda         # parte I (carga completa)
make CARGA=2 SUBST=0 roda # parte II (código por demanda)
make SUBST=1 LIMITE_QUADROS=20 roda   # parte III, FIFO (SUBST=2: LRU)
make testes               # testes nativos (gcc), FIFO e LRU
make relatorio            # matriz de configurações -> resultados/
```

## Projeto

- Cada programa (`usuario/*.c`) é montado sozinho a partir de 0, com
  `crt0.asm` primeiro. Texto e dados são separados e os dados começam na
  página seguinte ao texto (montagem em dois passos; `mkdisco -a` gera o
  `.org`). `ferramentas/mkdisco.c` grava um setor por página no disco,
  gera `build/programas_disco.h` e lista as instruções que cruzam páginas.
- Espaço virtual: código nas páginas `0..n_ro-1` (entrada de código tipo 1
  e espelho de dados tipo 1: escrever no código dá a int 1), dados tipo 3,
  pilha nas páginas 126–127 (`sp=0`), o resto tipo 0 (int 5).
- Quadros: mapa de 2048 bits; só os primeiros 64 KB são alcançáveis (núcleo
  com P=0 e ponteiros de 16 bits, DMA do disco e `pt` de 16 bits). O pool vai
  do fim da imagem do núcleo (quadro 61) até 0xD000: 43 quadros.
- Interrupção 5: na parte I toda falta mata. Na parte II, se a página do
  `ip` já está presente foi falta de dado (mata); se a página é ≥ `n_ro`, é
  execução fora do código (mata); senão carrega a página. Se o imediato de
  uma instrução cruza para a página ausente, o `ip` volta 2 e a página
  anterior não pode ser vítima.
- Substituição global só de páginas de código; dados não são expulsos (não
  voltariam por demanda). LRU por envelhecimento a cada tique (em assembly);
  página nova entra com idade 128. Tipo 3 seria gravado na área de troca:
  coberto pelos testes nativos, não ocorre no cenário.
- O `init` cria os processos em duas levas para a parte I caber nos 43 quadros.

## Resultados (`resultados/resumo.txt`)

Mínimo para concluir o cenário: **20 quadros** (FIFO e LRU) = tabela + dados
+ pilha dos processos simultâneos (18) + 2 quadros de código (cruzamento).
O LRU reduz as faltas do `paginas` (50 × 80 com 20 quadros), mas perde nos
processos em round-robin: as páginas de quem espera a vez são as "menos
recentes". O maior trecho no núcleo fica abaixo do `INTERVALO` (10000) em
todas as execuções (o maior é da parte I: 9945).
