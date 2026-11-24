#!/bin/sh
# relatorio.sh -- roda o SO em várias configurações (mesmo cenário de
# programas e a mesma entrada simulada) e guarda a saída de cada uma em
# resultados/, mais um resumo comparativo em resultados/resumo.txt.

set -e
cd "$(dirname "$0")"

ENTRADA="-e 300000:ola -e 900000:mundo -e 2000000:fim"
mkdir -p resultados

# nome escalonador quantum intervalo bloqueio
CONFIGS="
simples-q4       1  4 5000 1
rr-q4            2  4 5000 1
prio-q4          3  4 5000 1
rr-q1            2  1 5000 1
rr-q16           2 16 5000 1
prio-q1          3  1 5000 1
prio-q16         3 16 5000 1
rr-q2-i10000     2  2 10000 1
rr-q4-semblq     2  4 5000 0
"

printf '%-14s %9s %9s %6s %7s %9s %9s %9s %9s\n' \
  config total ocioso preemp nucleo% ret.calc ret.misto resp.eco resp.init > resultados/resumo.txt

echo "$CONFIGS" | while read nome esc q intv blq; do
  [ -z "$nome" ] && continue
  make -s ESCALONADOR=$esc QUANTUM=$q INTERVALO=$intv BLOQUEIO=$blq > /dev/null
  cfg=build/e$esc-q$q-i$intv-b$blq
  if ! ./bin/executor $cfg/so.mob $ENTRADA > resultados/$nome.txt 2>&1; then
    echo "Erro na execucao $nome; veja resultados/$nome.txt" >&2
    exit 1
  fi
  if ! grep -q '^\[executor\] halt apos ' resultados/$nome.txt ||
     ! grep -q '^================ METRICAS DO SO ' resultados/$nome.txt ||
     grep -q 'PANICO:' resultados/$nome.txt; then
    echo "Execucao $nome sem termino normal e metricas; veja resultados/$nome.txt" >&2
    exit 1
  fi
  echo "  $nome"

  awk -v nome="$nome" '
    /^tempo total:/    { total = $3 }
    /^tempo ocioso:/   { ocioso = $3 }
    /^preempcoes:/     { preemp = $2 }
    /^pid programa  retorno/ { tab = 1; next }
    tab && $2 == "calc"  { rcalc = $3 }
    tab && $2 == "misto" { rmisto = $3 }
    tab && $2 == "eco"   { reco = $5 }
    tab && $2 == "init"  { rinit = $5 }
    /vezes em estado/  { tab = 0 }
    /^\[executor\] nucleo/ { nuc = $3; oc = $5; us = $7; gsub(",", "", nuc); gsub(",", "", oc) }
    END {
      pct = (nuc + oc + us) > 0 ? 100 * nuc / (nuc + oc + us) : 0
      printf "%-14s %9s %9s %6s %6.1f%% %9s %9s %9s %9s\n", nome, total, ocioso, preemp, pct, rcalc, rmisto, reco, rinit
    }' resultados/$nome.txt >> resultados/resumo.txt
done

echo
cat resultados/resumo.txt
