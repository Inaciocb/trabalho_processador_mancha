**Alunos: Inácio Buemo e Kaio Fernandes**

As soluções usam o simulador original, sem alterar a CPU, os dispositivos ou
o Makefile. Toda a saída passa pela console simulada (`outb` na porta `0x01`).

## Arquivos e soluções

Os fontes e seus objetos `.mob` estão em
[simulador_completo/exemplos/](simulador_completo/exemplos/).
Cada programa é autocontido e pode ser montado separadamente. A pasta
`mancha/`, quando presente localmente, é um clone separado e não é necessária.

- **[gera_aleatorio.asm](simulador_completo/exemplos/gera_aleatorio.asm):**
  imprime um número entre 0 e 255 usando `x = (13 × semente + 7) mod 256`.
  A primeira semente combina o byte baixo do relógio com um caractere recebido
  pela console. O instante em que enviamos a entrada permite variar o resultado;
  repetições ainda são possíveis. Uma variável separada registra a inicialização,
  pois zero também é uma semente válida.
- **[mede_execucao.asm](simulador_completo/exemplos/mede_execucao.asm):**
  habilita o relógio e imprime a contagem de instruções e o tempo simulado até
  cinco segundos. Adotamos **1.000 instruções = 1 segundo simulado**; a duração
  está em `duracao` (1 a 30). As seis instruções anteriores à ativação do relógio
  são somadas para obter a contagem desde o início do programa.
- **[le_relogio.asm](simulador_completo/exemplos/le_relogio.asm):**
  lê dez valores do dispositivo de relógio e imprime em decimal. A constante
  `n` permite escolher de 1 a 30 leituras. Nesse intervalo, os valores já aparecem
  crescentes porque o contador avança durante a execução e a impressão.

O [teste_aleatorio.asm](simulador_completo/exemplos/teste_aleatorio.asm), já
existente no trabalho, foi ajustado para montar sozinho e imprimir dez valores
do gerador em hexadecimal. Ele também recebe a primeira entrada com `E` + Enter.

## Como chegamos à solução

O relógio original usa `0x20`/`0x21` para os bytes alto/baixo do contador e
`0x22`/`0x23` para seu limite. Ele começa desativado e conta uma unidade por
instrução. Lemos alto–baixo–alto e repetimos se o byte alto mudar, evitando
combinar bytes de instantes incompatíveis.

Corrigimos o endereço `0020` do gerador anterior: o montador interpreta esse
literal como octal, não como `0x20`. Usamos a console como entrada externa para
variar a semente, pois uma execução idêntica do relógio simulado é determinística.

Conforme a adaptação combinada, usamos o relógio existente como dispositivo de
leitura. O tempo é uma conversão da contagem de instruções, identificada como
`segundos_simulados` na saída; não mede o tempo real do computador.

## Montagem e execução

Requer compilador C, `make`, curses/ncurses e terminal com pelo menos 90 × 36.
A partir da raiz:

```sh
cd simulador_completo
make -B
./bin/simulador exemplos/gera_aleatorio.mob
# Para os outros desafios:
./bin/simulador exemplos/mede_execucao.mob
./bin/simulador exemplos/le_relogio.mob
```

`make -B` recompila as ferramentas na máquina do professor, sem depender dos
binários que já estavam versionados. Para conferir a sequência do gerador,
execute também `./bin/simulador exemplos/teste_aleatorio.mob`.

Digite cada comando seguido de Enter: `D0` seleciona a maior velocidade,
`C` executa e `F` sai. No gerador, **depois de iniciar com `C`, envie `E` + Enter**
para fornecer a entrada. Nos outros dois programas não é necessário fornecer dados.
A velocidade altera o tempo de espera real, não a conversão para segundos simulados.

## Verificação

Em uma cópia limpa do repositório, executamos `make -B` e os quatro programas
pela interface do simulador, seguindo os comandos acima. Em testes temporários,
conferimos a contagem contra a CPU, durações de 1, 5 e 30 segundos simulados,
`n = 1`, `10` e `30`, sementes diferentes e o ciclo completo de 256 valores do
LCG, incluindo zero. Os exemplos originais `soma`, `conta`, `protecao` e
`paginacao` também chegaram a `halt`. Nenhuma alteração no simulador foi necessária.

## T2 — implementação de processos

O mini sistema operacional do T2 está em [so/](so/): núcleo em C (mcc) sobre a
[BIOS](bios_mancha/), com processos, escalonadores simples, round-robin e por
prioridade, as cinco chamadas de sistema, bloqueio por E/S e por espera,
preempção pelo relógio, métricas e relatório de execuções. Compilação,
execução e a análise dos resultados estão em [so/README.md](so/README.md).
