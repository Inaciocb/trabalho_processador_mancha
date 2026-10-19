# T2 — Mini sistema operacional sobre o Mancha

Um sistema operacional pequeno, com multiprogramação, rodando sobre a
[BIOS](../bios_mancha/) no [simulador do Mancha completo](../simulador_completo/).
Ele implementa processos, três escalonadores, as cinco chamadas de sistema
pedidas, bloqueio por E/S e por espera de outro processo, preempção pelo
relógio e as métricas de execução — as Partes I, II e III do enunciado.

O núcleo é escrito em C (compilado pelo [mcc](../compilador_c/)). Uma ponte
pequena em assembly cuida do que o C do mcc não alcança.

## Sumário

1. [O que foi entregue](#o-que-foi-entregue)
2. [Como compilar e rodar](#como-compilar-e-rodar)
3. [Arquitetura](#arquitetura)
4. [Parte I — processos](#parte-i--processos)
5. [Parte II — bloqueio](#parte-ii--bloqueio)
6. [Parte III — preempção, escalonadores e métricas](#parte-iii--preempção-escalonadores-e-métricas)
7. [Chamadas de sistema e programas de usuário](#chamadas-de-sistema-e-programas-de-usuário)
8. [Escolhas e limitações](#escolhas-e-limitações)
9. [Relatório de execuções](#relatório-de-execuções)
10. [Validação](#validação)

## O que foi entregue

Arquivos criados nesta entrega (tudo em `so/`):

| arquivo | conteúdo |
|---|---|
| `src/so.c` | núcleo: tabela de processos, escalonadores, chamadas de sistema, pendências, métricas |
| `src/ponte.asm` | instala os pontos de entrada no vetor e nos ganchos da BIOS; portas, `halt`, laço ocioso, primeiro `rete` |
| `src/so_chamadas.h` | números das chamadas e dos programas (compartilhado núcleo/usuário) |
| `usuario/runtime_so.asm` | runtime dos processos: adaptação de `compilador_c/rt/runtime.asm` para usar chamadas de sistema |
| `usuario/so.h` | interface das chamadas para os programas em C |
| `usuario/programas.c` | programas de teste e a tabela de programas |
| `ferramentas/executor.c` | roda um `.mob` sem a interface curses (para execuções longas e para o relatório) |
| `relatorio.sh` | roda as configurações do relatório e gera `resultados/` |
| `resultados/` | saída de cada configuração e `resumo.txt` |

Mudanças fora de `so/`:

- `bios_mancha/` foi trazida do repositório da disciplina (`mancha-master`),
  junto com a versão mais nova do `mcc`, que tem ponteiros de função.
- Correção de um bug no `mcc` (`compilador_c/src/parser.c`): a string vazia
  `""` gerava uma linha `0` solta no assembly, e o montador recusava o arquivo.
- `simulador_completo/Makefile`: regra para montar o `teste_aleatorio.asm`
  do T1 junto com o `gera_aleatorio.asm`.
- `Makefile` da raiz: passa a compilar também `so/`.

## Como compilar e rodar

```sh
make                      # na raiz: simulador, mcc, BIOS e SO
cd so
make roda                 # roda sem interface, com entrada simulada
make relatorio            # roda todas as configurações do relatório
```

Configuração (cada combinação é compilada num diretório próprio em `build/`):

```sh
make ESCALONADOR=3 QUANTUM=2 INTERVALO=5000 BLOQUEIO=1
```

| variável | valores | padrão |
|---|---|---|
| `ESCALONADOR` | 1 = simples (Parte I), 2 = round-robin, 3 = prioridade | 2 |
| `QUANTUM` | em interrupções de relógio (1 a 32) | 4 |
| `INTERVALO` | instruções entre interrupções de relógio | 5000 |
| `BLOQUEIO` | 1 = E/S bloqueia (Parte II), 0 = devolve -1 e o processo tenta de novo | 1 |

**No simulador interativo:**

```sh
../simulador_completo/bin/simulador build/so.mob
```

Digite `D0` e `C` para rodar. O processo `eco` espera três linhas da console:
digite `Eola`, `Emundo` e `Efim` (o comando `E` do operador). Sem essa entrada,
o `eco` fica bloqueado para sempre e o sistema não termina — esse é o
comportamento correto. A janela da console no curses só mostra as últimas
linhas; para ver a saída inteira, incluindo as métricas, use o executor:

```sh
./bin/executor build/so.mob -e 300000:ola -e 900000:mundo -e 2000000:fim
```

`-e instante:texto` coloca `texto` (mais `\n`) na entrada da console quando o
simulador chega naquela instrução. No fim, o executor informa em stderr
quantas instruções rodaram no núcleo, no laço ocioso e em modo usuário, e o
maior trecho contínuo dentro do núcleo.

## Arquitetura

### Uma única porta de entrada

Toda interrupção que interessa ao SO termina em
`so_trata_interrupcao(irq, quadro)` ([so.c](src/so.c)). `quadro` é o
endereço fixo onde a CPU empilhou as 16 palavras do contexto interrompido
(`pilha_sistema-32`). A função faz sempre o mesmo roteiro:

```
trata a interrupção  ->  pendências  ->  escalona  ->  despacha  ->  rete
(chamada, relógio,      (desbloqueia      (escolhe o     (grava em "quadro"
 console, exceção)       quem puder)       próximo)       o contexto escolhido)
```

Como os pontos de entrada chegam até ela:

| origem | como chega | irq |
|---|---|---|
| `trap 7` | despachante da BIOS → `tabela_syscalls[1..5]` → `_so_entrada_chamada` | 7 |
| relógio | handler da BIOS → `retomada_escalonador` → `_so_entrada_relogio` | 10 |
| console | quadro 8 do vetor, reapontado para `_so_entrada_console` | 8 |
| exceções 1–5 | quadros 1–5 do vetor, reapontados | 1–5 |

O relógio e as chamadas de sistema usam os ganchos que a BIOS oferece. A
console e as exceções foram reapontadas no próprio vetor por dois motivos. O
núcleo precisa saber da chegada de caracteres para desbloquear quem espera
entrada. E uma exceção num processo de usuário deve matar só aquele
processo, não parar a máquina (a BIOS só imprime a exceção e para).

### Nenhum processo em execução

Quando ninguém está pronto, `atual = -1` e o núcleo retoma um **contexto
ocioso**: um laço `jmp` em modo supervisor com interrupções ligadas
(`_f_so_ocioso`). `halt` não serve para isso, porque no simulador ele para a
CPU de vez. A próxima interrupção traz o controle de volta ao núcleo, que
descarta esse contexto. É também assim que o tempo ocioso é medido.

### Salvamento preguiçoso do contexto

A primeira versão copiava as 16 palavras do quadro para a tabela em toda
entrada e de volta na saída. Um perfil por função mostrou isso, mais a
varredura da tabela em `trata_pendencias`, como os maiores custos do núcleo.
Agora:

- enquanto o processo interrompido continua na CPU, seus registradores
  ficam só no quadro vivo (`regs`). As chamadas de sistema leem argumentos e
  gravam resultados direto ali;
- o contexto só vai para a tabela quando o processo sai da CPU
  (`tira_da_cpu`, ao bloquear ou ser preemptado);
- o contexto só volta da tabela quando **outro** processo entra (`despacha`);
- `trata_pendencias` só percorre a tabela se há alguém bloqueado em E/S.

Com isso (e com a impressão do núcleo passando a usar as rotinas da BIOS),
as instruções gastas no núcleo caíram de 1,47 milhão para 0,65 milhão num
mesmo cenário de teste.

### Tempo em instruções

O relógio conta 1 por instrução e zera ao chegar no limite (`INTERVALO`),
gerando a interrupção 10. O instante atual é o total dos períodos já
completos mais o contador. Dois detalhes:

- `int` do mcc tem 16 bits (sem `long`/`unsigned`), então os tempos usam
  `struct t32 { a, b }`, valor = `a * 10000 + b`, com soma, subtração e
  divisão próprias.
- Se o contador virar enquanto o núcleo roda (interrupções desligadas), a
  interrupção do relógio fica pendente até o `rete`. `le_agora` percebe a
  virada por uma leitura menor que a anterior, adianta o período e marca
  isso, para não somá-lo de novo quando a interrupção chegar.

**`INTERVALO` precisa ser maior que o maior trecho contínuo do núcleo.** Se
não for, o contador vira duas vezes com interrupções desligadas, uma
interrupção se perde, e o tempo fica subcontado (é a "pegadinha" descrita
no README da BIOS). O executor mede esse trecho. Nas execuções do relatório
o maior foi de 3923 instruções; daí o padrão de 5000.

## Parte I — processos

| pedido | onde |
|---|---|
| estrutura com as informações de um processo | `struct processo`: estado, pid, programa, quadro de 16 palavras, motivo/argumento do bloqueio, prioridade, ligação na fila, índice das métricas |
| tabela de processos | `struct processo tabela[MAX_PROC]` (8 entradas) |
| primeira entrada criada com o init | `so_inicia` → `cria_processo(PROG_INIT)` |
| variável do processo em execução, aceitando "nenhum" | `atual` (índice ou -1) + contexto ocioso |
| salvar/recuperar o quadro de 16 palavras | `tira_da_cpu` (quadro → tabela) e `despacha` (tabela → quadro), com `bios_copia_quadro` |
| escalonador | `escalona` + `escolhe`; o simples (`ESCALONADOR=1`) mantém o atual se ele pode rodar, senão pega o primeiro pronto da tabela |
| criação e morte | `cria_processo` / `mata_processo` (`SO_CRIA_PROC`, `SO_MATA_PROC`) |
| pid ≠ entrada da tabela | `proximo_pid` só cresce; a entrada é reaproveitada. O núcleo registra `[SO] cria pid 7 (ola) na entrada 1` — a entrada que era do pid 2 |

Cada processo começa com `sr = 0`: `S=0` (usuário), `I=0` (sem E/S) e `D=0`
(interrupções ligadas). O topo da pilha dele guarda o endereço de
`fim_processo`, então, se a função do programa retornar, o processo cai numa
chamada `SO_MATA_PROC(0)`.

## Parte II — bloqueio

- `SO_LE`: se há caractere no buffer do núcleo, devolve na hora; senão o
  processo bloqueia (`B_LE`). `SO_ESCREVE` faz o mesmo com o bit "pronto
  para enviar" da console (`B_ESCREVE`).
- `trata_pendencias`, chamada em toda entrada no núcleo, completa a
  operação de quem estava bloqueado e o desbloqueia. O resultado vai para o
  `r0` salvo na tabela, que o processo vê quando voltar a rodar.
- A interrupção da console esvazia a porta num buffer de 32 caracteres (uma
  interrupção pode representar vários caracteres).
- `SO_ESPERA_PROC(pid)` bloqueia até `pid` morrer. Devolve -1 sem bloquear
  se `pid` não existe ou é o próprio chamador. Ao matar um processo,
  `mata_processo` acorda quem esperava por ele, com `r0 = 0`.

Com `BLOQUEIO=0`, as chamadas de E/S devolvem -1 em vez de bloquear, e o
runtime do usuário tenta de novo — a espera ocupada da Parte I. Isso fica
como configuração do relatório, para comparar.

## Parte III — preempção, escalonadores e métricas

**Round-robin** (`ESCALONADOR=2`): os prontos ficam numa fila encadeada
(`fila_ini`/`fila_fim`, campo `prox`). O escalonador pega o primeiro. Quem
fica pronto (criado ou desbloqueado) vai para o fim. `quantum_restante` é
reiniciado com `QUANTUM` a cada nova escolha e decrementado a cada
interrupção de relógio. Ao chegar em zero, o processo vai para o fim da fila
(preempção). Se não há ninguém na fila, o quantum só é renovado, e isso não
conta como preempção.

**Prioridade** (`ESCALONADOR=3`): mesmo quantum e preempção, mas escolhe o
pronto com menor `prio` (empate: quem está há mais tempo na fila). A
prioridade começa em 0,5. Quando o processo perde a CPU (bloqueio ou fim do
quantum), vira `prio = (prio + t_exec/t_quantum) / 2`, com
`t_exec = QUANTUM - quantum_restante`. Sem ponto flutuante no mcc, `prio` é
guardada em milésimos (0,5 = 500), e por isso o quantum vai até 32
(`t_exec * 1000` cabe em 16 bits).

**Métricas**, impressas quando o init morre (o SO então para):

| pedida | coluna/linha |
|---|---|
| processos criados | `processos criados` |
| tempo total de execução | `tempo total` |
| tempo ocioso | `tempo ocioso` (tempo com o contexto ocioso na CPU) |
| interrupções por tipo | `interrupcoes` |
| preempções (total e por processo) | `preempcoes`, coluna `preemp` |
| tempo de retorno de cada processo | `retorno` (término − criação) |
| vezes em cada estado | `pron bloq exec` |
| tempo em cada estado | `pronto bloqueado executando` |
| tempo médio de resposta | `resp.media (n)`: média entre desbloquear e ser escalonado, sobre `n` desbloqueios |

As métricas de cada processo ficam num vetor próprio (`registros`), porque a
entrada da tabela de processos é reaproveitada quando ele morre.

## Chamadas de sistema e programas de usuário

Convenção da BIOS: número em `r0`, argumento em `r1`, `trap 7`, resultado em `r0`.

| nº | chamada | argumento | resultado |
|---|---|---|---|
| 1 | `SO_LE` | — | caractere (ou -1 com `BLOQUEIO=0`) |
| 2 | `SO_ESCREVE` | caractere | 0 (ou -1 com `BLOQUEIO=0`) |
| 3 | `SO_CRIA_PROC` | número do programa | pid, ou -1 |
| 4 | `SO_MATA_PROC` | pid (0 = o próprio) | 0, ou -1 se não existe |
| 5 | `SO_ESPERA_PROC` | pid | 0 quando ele morre, ou -1 |

`usuario/runtime_so.asm` é a adaptação do `runtime.asm` do mcc que o
enunciado sugere: `mancha_out`, `mancha_in` e `mancha_halt` viram chamadas de
sistema. Assim a biblioteca do mcc (`putchar`, `getchar`, `puts`,
`print_int`), montada sem mudança, funciona em modo usuário.

O processo a criar é identificado por um número, que indexa a tabela de
programas em `usuario/programas.c` (o equivalente a um "nome de arquivo"
executável). O cenário de teste:

| programa | perfil |
|---|---|
| `init` | cria os outros, testa os erros de `SO_ESPERA_PROC`/`SO_CRIA_PROC`, espera, mata o `eterno`, cria o `ola` e sai |
| `calc` | limitado por CPU: conta primos abaixo de 900 sem nenhuma chamada no meio |
| `eco` | limitado por E/S: lê 3 linhas da console e as devolve em maiúsculas |
| `misto` | alterna um laço de cálculo com uma linha de saída |
| `eterno` | laço infinito; morto pelo `init` |
| `invasor` | tenta `out` direto → interrupção 2 → o SO mata só ele |
| `ola` | processo curto criado no fim, numa entrada reaproveitada |

## Escolhas e limitações

- **Sem isolamento de memória entre processos.** Todo o código (BIOS,
  núcleo, programas) é montado num único `.mob`, com `cs=ds=0` e limites
  zerados. Os processos não executam E/S nem instruções privilegiadas
  (`S=0`, `I=0`), mas conseguiriam escrever na memória de outros. Isolar
  memória (segmentos ou paginação por processo) é o assunto natural do
  trabalho seguinte.
- **A saída da console está sempre pronta** neste simulador (bit 0 da porta
  2 é fixo em 1). O caminho de bloqueio de `SO_ESCREVE` existe e é tratado
  nas pendências, mas nunca é exercitado. Quem bloqueia de fato por E/S é
  `SO_LE`.
- **Saída intercalada**: todos os processos escrevem na mesma console, um
  caractere por chamada, e podem ser preemptados no meio de uma linha. Por
  isso as linhas de processos diferentes aparecem misturadas.
- **Limites**: 8 processos simultâneos, pilha de 512 bytes por processo,
  métricas para os 24 primeiros processos, contadores de 16 bits (até
  32767 chamadas/interrupções de cada tipo), quantum até 32.
- **Fim do sistema** quando o init morre: processos ainda vivos aparecem
  como `(vivo)` nas métricas.
- **O custo do núcleo é alto** (30–55% das instruções, conforme a
  configuração), porque o mcc não otimiza nada e cada caractere escrito é
  uma chamada de sistema. A impressão do núcleo usa `bios_putc`/
  `bios_imprime`, bem mais baratas que o mesmo laço em C.

## Relatório de execuções

Mesmo cenário em todas (7 processos), com a entrada do `eco` chegando nas
instruções 300.000, 900.000 e 2.000.000. Tempos em instruções. `nucleo%` é a
fração das instruções executadas no núcleo, medida pelo executor. Saídas
completas em [`resultados/`](resultados/).

```
config             total    ocioso preemp nucleo%  ret.calc ret.misto  resp.eco resp.init
simples-q4       2060665   1199020      0   32.0%    308261    639336         0         0
rr-q4            2060777    703277     60   35.1%   1062021   1142281     43430      7728
prio-q4          2061167    697571     59   37.0%   1094355   1180501     11713         0
rr-q1            2066216    347370    316   50.8%   1444705   1491492     11548      1134
rr-q16           2060739    900953     10   28.3%    839671    893829     98331     29039
prio-q1          2067078    223015    341   56.3%   1509961   1635485      2503         0
prio-q16         2061092    938956      9   30.2%    865090    931805     28062      3415
rr-q2-i10000     2059105    788765     57   28.7%    982956   1059981     43450      6595
rr-q4-semblq     2058991         0     63   66.1%   1126821   1151313         -     12203
```

(`q` = quantum em interrupções; intervalo de 5000 instruções, exceto em
`i10000`; `semblq` = `BLOQUEIO=0`.)

**O tempo total é quase igual em todas** porque o sistema só termina depois
que o `eco` recebe a última linha (instrução 2.000.000). O que muda entre as
configurações é como esse tempo é dividido: tempo ocioso, de retorno e de
resposta.

**Simples × preemptivos.** Sem preempção, o `calc` roda até o fim de uma
vez (retorno de 308 mil, contra cerca de 1 milhão no round-robin), mas o
`eterno` nunca ganha a CPU: fica atrás na tabela e é morto ainda pronto
(0 instruções executadas). O `eco` tem resposta 0 por sorte do cenário: ele
só é acordado quando quem roda acabou de bloquear ou morrer. Se um processo
limitado por CPU estivesse rodando sem nunca bloquear, ninguém mais rodaria.

**Round-robin × prioridade.** O efeito pedido aparece no `eco`: com
prioridade, ele usa pouco de cada quantum (bloqueia logo), sua `prio` cai e
ele passa na frente quando acorda. Com quantum 4, a resposta média cai de
43.430 para 11.713 instruções; com quantum 1, de 11.548 para 2.503. O custo
fica com os limitados por CPU: o retorno do `calc` e do `misto` sobe um
pouco (≈3%).

**Tamanho do quantum.** Quantum menor dá respostas melhores (o `eco` com
q1 responde em 11,5 mil contra 98 mil com q16), mas multiplica as
preempções (316 contra 10) e o custo do núcleo (50,8% contra 28,3%). O
retorno dos limitados por CPU piora (`calc`: 1,44 milhão contra 840 mil). O
"tempo ocioso" também cai com quantum menor, mas não porque a CPU foi mais
bem usada: o núcleo passa a ocupar o tempo que antes sobrava.

**Intervalo do relógio.** `rr-q2-i10000` tem a mesma fatia de tempo que
`rr-q4` (20.000 instruções), com metade das interrupções de relógio. O
núcleo cai de 35,1% para 28,7%, e o `calc` termina antes (983 mil contra
1,06 milhão). A resposta do `eco` não muda, porque ela depende do tamanho
da fatia, não do número de tiques.

**Bloqueio × espera ocupada.** Sem bloqueio (`rr-q4-semblq`), o `eco` fica
chamando `SO_LE` em laço: 3350 chamadas de leitura contra 14, núcleo em
66,1% e tempo ocioso zero. A CPU está ocupada o tempo todo, mas sem fazer
nada útil — é exatamente o desperdício que a Parte II elimina. O `eco` nunca
bloqueia, então não tem tempo de resposta (`-`).

## Validação

- As 9 execuções do relatório terminam com o init morrendo, a impressão das
  métricas e `halt`. Em todas, para cada processo,
  `pronto + bloqueado + executando = retorno`, e a soma das preempções por
  processo é igual ao total — conferido por script sobre `resultados/`.
- O número de interrupções de relógio × intervalo bate com o tempo total
  medido pelo SO. A diferença para o total do executor é a impressão das
  métricas, que acontece depois da medição final.
- `invasor` é morto com `instrucao privilegiada` e o sistema continua;
  `SO_ESPERA_PROC` devolve -1 para o próprio pid, para um pid inexistente e
  para um processo que já morreu; `SO_CRIA_PROC` devolve -1 para um programa
  inexistente; `SO_MATA_PROC` de outro processo (`eterno`) funciona e
  acorda quem esperava.
- A mesma imagem roda no simulador interativo (curses) até as métricas e o
  `halt`, com a entrada digitada pelo operador (`E<texto>`).
- O maior trecho contínuo no núcleo ficou abaixo de 4000 instruções em todas
  as configurações, menor que o intervalo de 5000: nenhuma interrupção de
  relógio se perde.
