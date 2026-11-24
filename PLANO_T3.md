# Plano de ataque — T3 (Paginação e processos de usuário)

Documento para a sessão do Claude Code que vai implementar o T3 neste
repositório. Leia inteiro antes de escrever código. Ele registra **o que já
foi verificado no código-fonte** (não precisa reinvestigar), **as decisões de
projeto e o porquê**, **a ordem de implementação com critérios de pronto**
e **as armadilhas que vão aparecer**. Onde houver "VERIFICAR", confira antes
de confiar.

---

## 0. Regras de ouro (não negociáveis)

1. **T1 e T2 não podem mudar de comportamento.** Arquivos compartilhados
   ficam congelados:
   `simulador_completo/src/*`, `bios_mancha/src/bios.asm`, `compilador_c/*`,
   e **todo** o diretório `so/` (é a entrega do T2; `so/resultados/` precisa
   continuar reproduzível byte a byte).
2. O T3 mora num diretório novo, **`T3/`**, criado copiando `so/` como ponto
   de partida. Ele *reutiliza* os compartilhados por caminho relativo (`../`),
   do mesmo jeito que `so/Makefile` já faz.
3. Ferramentas novas ou alteradas (executor com disco, gerador de disco)
   ficam em `T3/ferramentas/`, nunca em `simulador_completo/`.
4. Se aparecer um bug real em código compartilhado: **pare**. Corrija num
   commit separado, rode a regressão completa da seção 9 e documente. Não
   misture com o commit do T3.
5. Trabalhe num branch `t3`. Faça um commit por etapa da seção 5. Não comite
   `build/`, `bin/` nem `disco.img`.
6. Todo comando de verificação da seção 9 tem que passar antes do merge.

---

## 1. O que já existe e é reaproveitado

- `so/src/so.c` (T2): tabela de processos, três escalonadores, cinco
  chamadas, bloqueio, preempção, métricas em `struct t32`, salvamento
  preguiçoso do contexto (`regs`, `interrompido`, `tira_da_cpu`,
  `despacha`). **Tudo isso continua valendo.** O T3 muda a criação e a morte
  de processos, o tratamento da interrupção 5 e o relógio (envelhecimento).
- `so/src/ponte.asm` **já** reaponta o quadro 5 do vetor
  (`st r0, (40)` → `_so_entrada_pagina`) e já faz a exceção matar só o
  processo culpado (`trata_excecao`). A Parte I, no que toca ao handler,
  é "só" acrescentar o diagnóstico da página.
- `bios_mancha/exemplos/paginacao_disco_c/` é o exemplo de referência:
  processo compilado pelo mcc, montado **sozinho**, convertido por
  `mob2bin`, carregado do disco para um quadro e executado com `ip=0`,
  `sr=0x4000` e `s4=pt`. Leia `kernel.c`, `ponte.asm`, `dados.asm` e o
  `Makefile` de lá.
- `so/ferramentas/executor.c` roda um `.mob` sem curses e mede o "maior
  trecho contínuo no núcleo". **Não monta disco**: vai precisar de `-D`.
- `so/tests/nucleo.c`: testes nativos (gcc) do núcleo, com portas e ponte
  simuladas. Siga o mesmo padrão no T3.

---

## 2. Fatos do hardware/ferramentas já verificados no código

Não reinvestigue; use.

| fato | onde |
|---|---|
| `sr`: S=0x8000, **P=0x4000**, I=0x2000, D=0x1000 | `cpu.h` |
| Quadro de 16 palavras: `r0..r4,bp,sp,ip` = índices 0–7; `sr`=8; **`s4`/`pt` = índice 12** | `bios_mancha/README.md` |
| Entrada: tipo = bits 15–14, acesso = bit 13 (0x2000), quadro = bits 12–0 | `cpu.c: traduz` |
| Busca de instrução usa as entradas `pagina`; dado usa `128+pagina`; **toda escrita é acesso a dado** | `traduz(…, dados, escrita)` |
| Interrupção 1 por escrita só se `!supervisor && escrita && tipo==1` | `traduz` |
| A CPU liga o bit de acesso no 1º acesso e promove 2→3 numa escrita. **Nunca desliga** (isso é com o SO) | `traduz` |
| `cpu_interrompe` **desliga P** ao entrar na interrupção (correção do commit c88af4d). Sem isso, um processo paginado interrompido trava a máquina | `cpu.c:359` |
| Falta na **1ª palavra** da instrução: `ip` salvo = endereço que faltou | `le_instrucao_palavra` |
| Falta no **imediato** (2ª palavra): `ip` salvo = endereço do imediato, porque a 1ª palavra já avançou o `ip`. **`rete` voltaria no meio da instrução** (ver 3.6) | `cpu_executa_1` + `le_instrucao_palavra` |
| Falta de dado: `ip` salvo = instrução seguinte; endereço do operando perdido | enunciado e `cpu.c` |
| **DMA do disco tem endereço de 16 bits** (`uint16_t endereco`) | `dispositivos.c` |
| Disco: 2 faces × 40 trilhas × 18 setores × 512 B = 1440 setores. Deslocamento = `((face*40+trilha)*18+setor)*512` | `dispositivos.c` |
| `disp_destroi` **regrava o arquivo de imagem** do disco ao sair | `dispositivos.c` |
| `bios_disco_le_setor`/`escreve_setor`: r0=face, r1=trilha, r2=setor, r3=endereço; síncronas (laço `inb`); sujam r0–r4 | `bios.asm` |
| `pilha_nucleo = 0xE000` (cresce para baixo), `pilha_sistema = 0xF000` (quadro em 0xEFE0) | `bios.asm` |
| Montador: **um só contador de endereço** para todos os arquivos e segmentos, na ordem da linha de comando. `.text`/`.data` só marcam T/D no `.mob` | `montador.c` |
| Montador **não faz aritmética** em `.org` (só `símbolo±constante`, `$`). Não dá para escrever `.org alinhado(…)` | `montador.c: resolve_expr` |
| mcc emite por arquivo: `.text` + funções, depois `.data` + globais (`.db 0` para não inicializadas) + literais de string. Funções na ordem do fonte | saída real do mcc |
| mcc: `int` de 16 bits **com sinal**; sem `unsigned`/`long`/`switch`; `>>` é **lógico**; `#if` funciona; ponteiro de função funciona | `compilador_c/README.md`, `so.c` |
| Imagem atual do SO do T2 termina em ~0x6D59 (quadro 54), contando os programas e `pilhas[8][256]` (4 KB) | medido nos `.mob` |

**VERIFICAR:** se o `cpu.c` do repositório do professor (gitlab) também tem a
correção do bit P. O T3 depende dela. Se não tiver, documente no README que o
T3 exige o simulador deste repositório.

---

## 3. Decisões de projeto (com o raciocínio)

### 3.1 Só os primeiros 64 KB são alcançáveis: o pool real tem menos de 128 quadros

O enunciado fala em 1 MB (2048 quadros), mas, **sem alterar o simulador**:

- o núcleo roda com P=0, `ds=0` e ponteiros de 16 bits: só enxerga 0..0xFFFF;
- o DMA do disco só aceita destino de 16 bits;
- `pt` (`s4`) é um endereço físico de 16 bits.

Logo, todo quadro de usuário e toda tabela de páginas ficam abaixo de 64 KB.

Pool de quadros de usuário: `[ceil(fim_da_imagem/512), 0xD000/512 = 104)`.
O intervalo 0xD000–0xDFFF fica reservado para a pilha do núcleo, que desce a
partir de 0xE000; 0xE000–0xFFFF fica intocado (pilha do sistema). Depois de
tirar os programas e as `pilhas[]` de dentro da imagem, o núcleo deve
terminar perto de 0x5000, o que dá cerca de 60 quadros. Sobra para o
relatório, que de qualquer jeito pede **limitar** os quadros.

Para honrar o enunciado sem mentir: mantenha um **bitmap de 2048 bits**
(128 `int`), marque os quadros fora do pool como ocupados e explique no
README por que eles são inalcançáveis, citando as três razões acima.

O fim da imagem vem de um `fim.asm`, montado **por último**, com o rótulo
`_f_so_fim:`. No C, declare `void so_fim(void);` e use `(int) so_fim`, o
mesmo truque do T2 com `fim_processo`. Na inicialização, entre em pânico se
`primeiro_quadro >= 104`.

### 3.2 Layout do espaço virtual do processo

O problema é que o mcc mistura código e dados no mesmo espaço de endereços.
Strings e globais ficam em endereços como 0x0450, mas toda leitura e escrita
deles passa pelas entradas de **dados** (`128+página`). Por isso o layout do
enunciado ("código em 0..N-1, dados a partir de 128 como área separada")
não funciona sozinho: as strings sairiam lixo. Usar um layout "tudo
leitura/escrita" também não serve: quebra o teste "escrita no código → int 1"
e obriga as páginas de código a ficarem residentes via dados, o que anula a
Parte II.

A decisão é **um espaço virtual unificado, com a imagem dividida em duas
partes alinhadas a página**:

```
virtual 0 ........ A-1 | A ........ B-1 | (buraco = guarda) | 126*512 .. 0xFFFF
[ RO: só instruções ]  [ RW: .data/strings/globais ]          [ pilha: 2 páginas ]
```

| região | entradas de código (0–127) | entradas de dados (128–255) |
|---|---|---|
| RO, páginas `0..n_ro-1` | tipo 1 → quadro q | **tipo 1 → o mesmo quadro q** (espelho; dá a int 1 na escrita) |
| RW, páginas `n_ro..n_ro+n_rw-1` | **tipo 0** (executar dado → int 5) | tipo 3, quadros próprios carregados do disco |
| pilha, páginas 126 e 127 | tipo 0 | tipo 3, quadros zerados |
| todo o resto | tipo 0 | tipo 0 |

Os quadros RO e RW de cada processo são **cópias próprias**, sem
compartilhamento entre processos. Isso simplifica o dono de cada quadro na
Parte III.

O quadro inicial tem `ip=0`, **`sp=0`** (o 1º `push` faz 0−2 = 0xFFFE,
página 127), `sr=0x4000` (S=0, P=1, I=0, D=0), `quadro[12]=pt` e o resto
zerado. Um estouro da pilha para baixo da página 126 cai no buraco → int 5 →
o processo morre. É uma proteção de graça.

**Como conseguir a divisão com o montador**, que não alinha por expressão:

1. Separe cada `.asm` gerado pelo mcc em `X_text.asm` e `X_data.asm`. Um
   `awk` simples alterna o arquivo de saída em cada linha `.text`/`.data`.
   Os `.asm` escritos à mão (`crt0.asm`, `runtime_so.asm`) são só texto.
2. **Passo 1:** monte só os textos, na ordem
   `crt0.asm prog_text.asm ulib_text.asm runtime_so.asm biblioteca_text.asm`,
   para medir o tamanho. O ideal é montar tudo com `.org 0x7E00` antes dos
   dados (os símbolos de dados precisam existir) e medir o maior endereço T.
3. Calcule `A = ceil(fim_texto/512)*512` e gere `alinha.asm` contendo
   `.org A`.
4. **Passo 2:** monte
   `crt0 … biblioteca_text alinha.asm prog_data ulib_data biblioteca_data`.
5. Valide no `.mob`: todo byte T < A e todo byte D ≥ A. Se falhar, aborte o
   build.

O `crt0.asm` vem **primeiro**, para o endereço 0 ser a entrada:
`_inicio: call _f_main` → `ld r1, 0` → `ld r0, SC_MATA` → `trap 7` →
`jmp _inicio` (nunca deveria voltar). Cada programa vira um `.c` próprio com
`int main(void)`. As funções auxiliares comuns (`u_escreve`,
`u_escreve_int`) vão para um `usuario/ulib.c`, compilado e montado junto
com cada programa.

### 3.3 Formato do disco e o gerador `mkdisco`

Escreva uma ferramenta nativa, `T3/ferramentas/mkdisco.c`, compilada contra
`objeto.c`, `memoria.c` e `instrucao.c` do simulador (como o `executor`).
Ela:

- recebe a lista ordenada de `.mob` dos programas (a ordem define o número
  do programa; **init = 0**);
- para cada um, descobre `A`, `B`, `n_ro = A/512`, `n_rw = ceil((B-A)/512)`
  e verifica que `n_ro + n_rw <= 126`;
- grava a imagem achatada (`n_ro + n_rw` setores) a partir de um **setor
  linear** inicial, com os programas contíguos no `disco.img`;
- faz a **decodificação linear** da região RO, a partir de 0, com
  `instrucao_decodifica` e `instrucao_requer_imediato`. Se uma palavra não
  decodificar ou houver desalinhamento, é erro: tem dado no meio do texto.
  Registre as páginas `k` em que a instrução que começa em `k*512-2`
  precisa de imediato. São os "cruzamentos" da seção 3.6;
- gera `build/programas_disco.h` com vetores **achatados**
  (`prog_setor[]`, `prog_n_ro[]`, `prog_n_rw[]`, `prog_cruza[]` com −1 como
  terminador, um nome por programa). **VERIFICAR** se o mcc aceita vetores
  globais de `int` inicializados. Se não aceitar, gere funções com cadeias
  de `if`, como o `programa_nome` do T2;
- imprime um relatório: páginas por programa e o mapa `_f_*` → página.
  Ajuda a montar o teste de localidade da Parte III.

No núcleo, a conversão do setor linear `s` é: `face = s / 720`,
`trilha = (s / 18) % 40`, `setor = s % 18`.

### 3.4 Acesso à memória física isolado em funções da ponte

Todo acesso a uma tabela de páginas ou a um quadro passa por funções
pequenas em `ponte.asm`:

```c
int  pt_le(int q_tabela, int indice);          // palavra em q_tabela*512 + indice*2
void pt_escreve(int q_tabela, int indice, int valor);
void zera_quadro(int q);                         // 256 palavras, laço em asm
void disco_le(int setor_linear, int q);          // converte e chama bios_disco_le_setor(…, q*512)
void disco_escreve(int setor_linear, int q);
int  disco_erro(void);
```

Três motivos:

- **Endereço físico ≥ 0x8000 é negativo no `int` do mcc.** Comparar
  endereços em C dá errado. Regra: no C só se manipulam **números de
  quadro** (0..127) e índices. Multiplicar por 512 acontece no asm, com
  `shl 9`.
- **Custo:** zerar 512 bytes num laço do mcc leva milhares de instruções.
  Em asm (`st r1, (r0)+`) fica em torno de 800.
- **Testabilidade:** nos testes com gcc, essas funções viram simulações
  sobre um vetor (padrão do `tests/nucleo.c`). Com ponteiros físicos
  diretos, o teste nativo faria acesso inválido no host.

Macros:

```c
#define TIPO(e)      (((e) >> 14) & 3)
#define ACESSO       0x2000
#define QUADRO(e)    ((e) & 0x1FFF)
#define ENTRADA(t,q) (((t) << 14) | (q))
```

Nunca compare entradas com `<` ou `>`: valores de tipo 2 ou 3 são negativos.

### 3.5 Gerenciador de quadros (pense na Parte III desde a Parte I)

```c
struct quadro_info {
    int uso;      // Q_LIVRE, Q_SO, Q_TABELA, Q_RO (código), Q_RW, Q_PILHA
    int dono;     // índice na tabela de processos, -1 se não tem
    int pagina;   // página virtual 0..127
    int idade;    // LRU: 0..255 (sem unsigned no mcc: 8 bits num int)
    int prox;     // fila FIFO dos quadros de código, na ordem de carga
};
```

- Uma tabela de páginas ocupa **um quadro do pool** (256 × 2 = 512 bytes).
  Ela não precisa de alinhamento, mas alocar um quadro simplifica.
- `aloca_quadro(uso, dono, pagina)`: pega um livre e respeita
  `LIMITE_QUADROS`, contando **todos** os quadros de usuário (tabelas, RW,
  pilha, RO). Sem livre, só a Parte III libera: escolhe uma vítima **entre
  os quadros `Q_RO`** (ver 3.7).
- `SO_CRIA_PROC` precisa reservar tabela + RW + pilha. Se não couber,
  devolve −1 e desfaz o que já alocou. Nunca expulsa dado para isso.
- `SO_MATA_PROC` / `mata_processo` devolve **todos** os quadros do processo,
  inclusive a tabela, e os tira da fila FIFO. Só aqui um quadro `Q_RW` ou
  `Q_PILHA` sai.
- Na Parte I (`CARGA=1`) os RO são carregados todos na criação. Na Parte II
  (`CARGA=2`), nenhum: as entradas de código começam tipo 0.

### 3.6 Instrução que cruza a fronteira de página (o bug que vai aparecer)

Uma instrução com imediato que começa em `k*512-2` tem a 1ª palavra na
página k−1 e o imediato na página k. Se a página k estiver ausente, a falta
acontece **com `ip = k*512`** (já avançado), e o `rete` retomaria a execução
lendo o imediato como se fosse uma instrução. Com o mcc quase toda instrução
tem imediato, então isso acontece **com certeza** em qualquer programa com
mais de uma página de código, já na Parte II. Sob pressão de memória, na
Parte III, vira um "heisenbug".

A ambiguidade não se resolve olhando a memória em tempo de execução: a
palavra em `ip-2` pode ser o imediato de outra instrução. Por isso o
`mkdisco` calcula os cruzamentos na hora do build (seção 3.3), e o núcleo
faz:

```c
// ip salvo com deslocamento 0 e página listada como cruzamento:
// a falta foi no imediato, então retoma no começo da instrução
if ((ip & 511) == 0 && cruza(prog, pag)) {
    regs[Q_IP] = ip - 2;            // grave no quadro vivo
    fixa = pag - 1;                 // a página k-1 não pode ser a vítima agora
}
```

Ao escolher a vítima para carregar k, **exclua a página k−1 do mesmo
processo**. As duas precisam estar residentes juntas, senão a execução entra
em laço. Consequência: o mínimo teórico é de **2 quadros de código** no
sistema.

### 3.7 Tratamento da interrupção 5 (Partes I e II)

```c
void trata_falta(int *quadro) {
    ip  = quadro[Q_IP]; pag = (ip >> 9) & 127;
    e   = pt_le(tabela[atual].pt, pag);
    if (CARGA == 1)                       -> mata("falta de pagina", pag)   // Parte I: toda falta é erro
    if (TIPO(e) != 0)                     -> mata("dado fora da area", ...) // a busca teria funcionado: foi acesso a DADO
    if (pag >= prog_n_ro[prog])           -> mata("codigo fora do programa", pag) // inclui saltar para dados/pilha
    /* falta legítima de código */
    ajusta cruzamento (3.6);
    q = aloca_quadro(Q_RO, atual, pag)    // pode expulsar (Parte III); se -1 -> mata("sem memoria")
    disco_le(prog_setor[prog] + pag, q);  // síncrono: a máquina inteira espera
    pt_escreve(pt, pag,       ENTRADA(1, q));
    pt_escreve(pt, 128 + pag, ENTRADA(1, q));   // espelho só leitura
    registros[...].faltas++ ; n_faltas++;
    // não troca de processo: "atual == interrompido", despacha não copia nada
}
```

- O teste "a entrada de código de `página(ip)` já está presente → foi falta
  de dado" é mais robusto que a sugestão do enunciado (comparar `ip` com as
  páginas de dados). Faça os dois: `pag >= n_ro` cobre a sugestão.
- Limitação conhecida, a documentar: se uma instrução **que termina
  exatamente no fim de uma página** fizer um acesso de dado ilegal e a
  página seguinte de código ainda não tiver sido carregada, o caso parece
  falta de código. A página é carregada e a instrução culpada é pulada.
  É raro e não tem solução sem decodificar a instrução.
- O diagnóstico ao matar repete o do T2 e acrescenta a página virtual e o
  tipo da falta.

### 3.8 Substituição (Parte III)

- Vítimas: **só quadros `Q_RO`**, de qualquer processo (substituição
  global), menos a página fixada (3.6). Os dados não voltam por demanda
  (limitação do enunciado), então nunca são expulsos.
- **FIFO**: o primeiro da fila de quadros de código, na ordem de carga.
  Use fila encadeada, como a de prontos do T2. Um contador de sequência de
  16 bits estouraria.
- **LRU por envelhecimento**: a cada interrupção do relógio, para cada
  quadro `Q_RO`, `idade = (idade >> 1) | (acessou ? 128 : 0)`, e depois
  desliga o bit 0x2000 da entrada de código do dono. A vítima é a de menor
  idade; no empate, a mais antiga na fila FIFO.
  **Armadilha:** em `trata_relogio` do T2 tudo está dentro de
  `#if ESCALONADOR != 1`. O envelhecimento precisa ficar **fora** desse
  `#if` e rodar antes do teste de quantum.
- Ao liberar: leia `TIPO` da entrada do dono. Tipo 3 → `disco_escreve` numa
  área de troca (setores depois dos programas, um bloco por slot da
  tabela). Tipos 1 e 2 → descarta. Depois zere a entrada de código **e** o
  espelho `128+pag` **na tabela do dono**, não na do processo que causou a
  falta. Com vítimas só `Q_RO`, o ramo do tipo 3 nunca roda na prática:
  implemente assim mesmo e **cubra com teste nativo**, explicando no README.
- Métricas novas: faltas por processo (coluna nova na tabela de métricas),
  faltas totais, substituições, escritas de volta e o pico de quadros
  usados.

### 3.9 O custo do núcleo e o `INTERVALO`

O T2 já mostrou que, se um trecho do núcleo com interrupções desligadas
passar do `INTERVALO`, uma interrupção do relógio se perde e o tempo fica
subcontado. No T3 aparecem trechos novos e longos:

- criação na Parte I: vários setores + zerar a tabela + zerar 2 páginas de
  pilha + impressão;
- a falta de página (leitura de setor);
- o envelhecimento a cada tique.

Use **`INTERVALO=10000` como padrão no T3** e confira o "maior trecho
contínuo no núcleo" que o executor imprime em **todas** as configurações do
relatório. Se passar, otimize (zerar e envelhecer em asm) ou aumente o
`INTERVALO`, e documente a escolha.

---

## 4. Configuração do build do T3

Mantenha as variáveis do T2 (`ESCALONADOR`, `QUANTUM`, `INTERVALO`,
`BLOQUEIO`) e acrescente:

| variável | valores | padrão |
|---|---|---|
| `CARGA` | 1 = completa (Parte I), 2 = por demanda (Parte II) | 2 |
| `SUBST` | 0 = nenhuma, 1 = FIFO, 2 = LRU | 2 |
| `LIMITE_QUADROS` | máximo de quadros de usuário (0 = o pool todo) | 0 |
| `INTERVALO` | igual ao T2 | **10000** |

Diretório por configuração, como no T2 (`build/e2-q4-i10000-b1-c2-s2-l0/`).
O `disco.img` é **um só** para todas, porque não depende da configuração.

---

## 5. Roteiro de implementação (uma etapa por commit, cada uma com critério de pronto)

**Etapa 0 — Linha de base.** Crie o branch `t3`. Rode a regressão da seção
9 e guarde as saídas esperadas do T1 em `T3/tests/esperado_t1/`. *Pronto:*
tudo passa antes de qualquer mudança.

**Etapa 1 — Esqueleto.** Copie `so/` para `T3/` (sem `build/`, `bin/` e
`resultados/`). Acrescente `T3` ao `Makefile` da raiz. *Pronto:* `make` na
raiz compila; `T3/` roda o mesmo cenário do T2; `so/` sem nenhum diff.

**Etapa 2 — Programas de usuário isolados.** `crt0.asm`, a separação
texto/dados, a montagem em dois passos, `mkdisco` e `disco.img`. Converta os
programas do T2 em `.c` separados com `main()`. *Pronto:* `mkdisco` lista
cada programa com `n_ro`, `n_rw` e cruzamentos; a validação T<A≤D passa;
`programas_disco.h` compila no mcc.

**Etapa 3 — Executor com disco.** Acrescente `-D imagem` ao executor do T3
(`disp_monta_disco(disp, 0, …)`). Como `disp_destroi` regrava a imagem, o
`Makefile` e o script do relatório **copiam um `disco.img` limpo para cada
execução**. *Pronto:* o executor monta e lê o disco; o original continua
intacto depois de rodar.

**Etapa 4 — Parte I.** Gerenciador de quadros, acessores da ponte (3.4),
criação com carga completa, morte liberando tudo, `sr=0x4000`,
`quadro[12]=pt`, `sp=0`, e a falta de página sempre mata. Tire os programas
e as `pilhas[]` da imagem do núcleo. *Pronto:* o cenário do T2 roda inteiro
em modo paginado; as métricas aparecem; os quadros livres no fim são iguais
aos do início (sem vazamento); o maior trecho no núcleo fica abaixo do
`INTERVALO`.

**Etapa 5 — Proteção e o mesmo binário.** Programas de teste da seção 6.
*Pronto:* cada invasor morre com a interrupção esperada (1, 2 ou 5) e o
sistema continua; duas instâncias de `calc` chegam ao mesmo valor, cada uma
no seu contador, no mesmo endereço virtual.

**Etapa 6 — Parte II.** `CARGA=2`, `trata_falta` (3.7), cruzamentos (3.6) e
faltas por processo. *Pronto:* o mesmo cenário conclui; as faltas por
processo são pelo menos o número de páginas de código realmente executadas;
o programa grande passa por cruzamentos sem corromper nada (confira com o
relatório do `mkdisco`).

**Etapa 7 — Parte III.** `LIMITE_QUADROS`, substituição FIFO,
envelhecimento + LRU, liberação com escrita de volta e testes nativos.
*Pronto:* com limite apertado o cenário conclui com mais faltas; os testes
nativos cobrem a escolha de vítima (FIFO e LRU), a página fixada, a escrita
de volta do tipo 3 e a anulação da entrada na tabela do dono.

**Etapa 8 — Relatório e README.** `T3/relatorio.sh`, `T3/resultados/` e
`T3/README.md` no estilo do `so/README.md`. *Pronto:* o script falha se uma
execução não terminar com `halt` + métricas, ou se houver pânico.

**Etapa 9 — Regressão final** (seção 9) e merge.

---

## 6. Programas de teste

| programa | objetivo |
|---|---|
| `init` | cria os demais, testa os erros (do T2), espera, mata o `eterno`, cria o `ola` numa entrada reaproveitada |
| `calc` ×2 | **o mesmo binário em dois processos**: incrementa um global até N e imprime o valor e o endereço (mesmo endereço virtual, valores independentes) |
| `paginas` | código grande (8 a 10 páginas) com localidade controlada: uma função "quente" chamada no laço e funções "frias" espalhadas, chamadas de vez em quando. Use o mapa `_f_*` → página do `mkdisco` para posicioná-las. LRU mantém a quente; FIFO a expulsa periodicamente |
| `escreve_codigo` | `*(int *) 0x0010 = 1;` → **int 1** (espelho só leitura) |
| `le_longe` | `x = *(int *) 0x7000;` → **int 5**, falta de dado |
| `salta_longe` | chama um ponteiro de função `0x6000` → **int 5**, código além de `n_ro` |
| `invasor` | o `out` do T2 → int 2 |
| `eco` | E/S (do T2) |

Os processos não sabem o próprio pid (não há essa chamada e as cinco são
fixas no enunciado): não acrescente chamadas.

---

## 7. Relatório (o que medir e como evitar o erro do T2)

- **Lição do T2:** lá o tempo total ficou preso no instante da última
  entrada do `eco` (2.000.000 em todas as configurações). Para comparar
  tempo de execução entre limites e algoritmos, o cenário do relatório do
  T3 **não pode depender de entrada tardia**: use um cenário sem `eco` ou
  com a entrada logo no início.
- Matriz: `CARGA=1` (referência), `CARGA=2` sem limite, e
  {FIFO, LRU} × `LIMITE_QUADROS` decrescente (por exemplo, o pool todo, 40,
  32, 28, 24, 20, 18, 16…) até o cenário não concluir.
- Por execução: faltas totais e por processo, substituições, escritas de
  volta, tempo total (instruções), retorno de cada processo, % de núcleo,
  maior trecho no núcleo, pico de quadros, e "concluiu?". A não conclusão
  é detectada pelo `-n` do executor ou por `SO_CRIA_PROC` devolvendo −1.
- **O limite mínimo** deve ser explicado na teoria e confirmado no
  experimento: a soma, sobre os processos simultâneos, de
  (1 tabela + n_rw + 2 pilha), **mais 2 quadros de código** por causa dos
  cruzamentos (3.6). Abaixo disso, ou a criação falha (dados não são
  expulsos) ou a execução entra em laço.
- Compare FIFO × LRU e mostre o efeito do `paginas`.

---

## 8. Armadilhas (checklist antes de cada commit)

- [ ] Nada mudou em `simulador_completo/`, `bios_mancha/src/`,
      `compilador_c/` ou `so/`.
- [ ] No C, só números de quadro e índices; nada de endereço físico ≥ 0x8000
      em comparação.
- [ ] Nenhuma comparação `<` ou `>` com entradas de tabela (tipos 2 e 3 são
      negativos).
- [ ] `crt0.asm` é o primeiro arquivo da montagem de cada programa (entrada
      em 0).
- [ ] Nenhum `.org` escrito à mão nos programas; só o `alinha.asm` gerado.
- [ ] Toda mudança de mapeamento escreve **as duas** entradas (código e
      espelho `128+pag`) e na tabela **do dono**.
- [ ] Cruzamento: `ip -= 2` e a página k−1 fixada durante a escolha da
      vítima.
- [ ] Envelhecimento fora do `#if ESCALONADOR != 1`.
- [ ] `disco.img` copiado limpo para cada execução.
- [ ] Maior trecho no núcleo < `INTERVALO` em todas as execuções.
- [ ] Quadros livres no fim = quadros livres no início (sem vazamento).
- [ ] `SO_CRIA_PROC` desfaz as alocações parciais quando falha.
- [ ] As chamadas de sistema continuam recebendo só inteiros. Se algum dia
      passarem ponteiros, o núcleo terá de traduzir virtual → físico pela
      tabela do processo; não faça isso agora.
- [ ] Os vetores do mcc e as strings do núcleo não estouram 16 bits
      (contadores de faltas ≤ 32767).

---

## 9. Regressão T1/T2 (rode no começo, depois de cada etapa que mexe em build, e no fim)

```sh
# na raiz
make -B

# T2 intacto: o relatório precisa sair idêntico ao versionado
cd so
make relatorio
git diff --exit-code -- resultados/        # tem que sair sem diff
gcc -std=gnu11 -Wno-pointer-to-int-cast -Itests tests/nucleo.c -o /tmp/tn && /tmp/tn
#   esperado: "OK: entrada sem descarte, limite de metricas, escrita bloqueada e leitura completada na mesma entrada"

# T1 intacto (saídas esperadas medidas em 23/11/2026)
E=./bin/executor; S=../simulador_completo/exemplos
$E $S/gera_aleatorio.mob -e 1000:E  2>/dev/null | tail -1   # "Envie E + Enter para gerar: 41"
$E $S/gera_aleatorio.mob -e 5000:E  2>/dev/null | tail -1   # "...: 214"
$E $S/le_relogio.mob               2>/dev/null | tail -1   # "5 51 122 218 314 410 506 602 698 794"
$E $S/mede_execucao.mob            2>/dev/null | tail -1   # "segundos_simulados=5 instrucoes=5008"
```

Se qualquer item mudar, o T3 tocou em algo compartilhado: volte e ache o
quê antes de seguir.

---

## 10. O que NÃO fazer

- Não alterar o simulador para "ganhar" 1 MB (DMA de 32 bits, núcleo
  paginado etc.). Quebra a premissa "sem alterar o simulador" do T1 e
  diverge do simulador do professor.
- Não carregar dados por demanda nem expulsar páginas de dados (o enunciado
  exclui isso, pois o endereço da falta não é conhecido).
- Não compartilhar quadros de código entre processos do mesmo binário. Não
  é pedido e complica o dono do quadro na Parte III.
- Não acrescentar chamadas de sistema.
- Não reescrever o escalonador nem as métricas do T2: o `pt` já viaja no
  quadro de 16 palavras, e o enunciado diz explicitamente que o escalonador
  não muda.
- Não editar `so/` "só para aproveitar". O T2 é entrega fechada.
