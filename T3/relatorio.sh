#!/bin/sh
# relatorio.sh -- roda o SO do T3 em várias configurações de memória (mesmo
# cenário de programas e a mesma entrada simulada, que chega logo no início
# para o tempo total não depender dela) e guarda a saída de cada uma em
# resultados/, mais um resumo comparativo em resultados/resumo.txt.
#
# Cada execução usa uma cópia limpa do disco (o simulador regrava a imagem
# ao sair). O script falha se uma execução não terminar com "halt" e as
# métricas, ou se houver pânico. Uma execução que termina mas não conclui o
# cenário (criação recusada por falta de memória, processo morto sem quadro
# para o código, ou saída esperada faltando) aparece com concluiu=nao.

set -e
cd "$(dirname "$0")"

ENTRADA="-e 1000:ola -e 1000:mundo -e 1000:fim"
LIMITES="0 36 30 26 24 22 21 20 19 18 17 16"
mkdir -p resultados

make -s all > /dev/null

printf '%-10s %-3s %6s %6s %5s %9s %7s %7s %5s %9s %9s %6s %6s\n' \
  config ok? faltas subst escr total nucleo% trecho pico ret.calc ret.pag flt.cal flt.pag > resultados/resumo.txt

# nome carga subst limite
roda() {
  nome=$1; carga=$2; subst=$3; lim=$4
  make -s ESCALONADOR=2 QUANTUM=4 INTERVALO=10000 BLOQUEIO=1 \
       CARGA=$carga SUBST=$subst LIMITE_QUADROS=$lim all > /dev/null
  cfg=build/e2-q4-i10000-b1-c$carga-s$subst-l$lim
  cp build/disco.img $cfg/disco.img
  if ! ./bin/executor $cfg/so.mob -D $cfg/disco.img $ENTRADA > resultados/$nome.txt 2>&1; then
    echo "Erro na execucao $nome; veja resultados/$nome.txt" >&2
    exit 1
  fi
  if ! grep -q '^\[executor\] halt apos ' resultados/$nome.txt ||
     ! grep -q '^================ METRICAS DO SO ' resultados/$nome.txt ||
     grep -q 'PANICO:' resultados/$nome.txt; then
    echo "Execucao $nome sem termino normal e metricas; veja resultados/$nome.txt" >&2
    exit 1
  fi
  # concluiu: init chegou ao fim, nenhuma criação recusada, os 12 processos
  # terminaram e só morreram por exceção os 4 testes de proteção (invasor,
  # escreve_codigo, le_longe, salta_longe). As mensagens do núcleo saem
  # inteiras (o núcleo roda com interrupções desligadas); as dos processos
  # podem se intercalar caractere a caractere, então só o "paginas" (que
  # roda sem outro processo escrevendo) tem o resultado conferido.
  ok=sim
  if ! grep -q '^init: fim' resultados/$nome.txt ||
     grep -q 'ERRO\|sem memoria' resultados/$nome.txt ||
     [ "$(grep -c '^\[SO\] fim pid' resultados/$nome.txt)" -ne 12 ] ||
     [ "$(grep -c '^\[SO\] processo .* morto' resultados/$nome.txt)" -ne 4 ] ||
     ! grep -q 'paginas: resultado 201' resultados/$nome.txt; then
    ok=nao
  fi
  echo "  $nome: concluiu=$ok"

  awk -v nome="$nome" -v ok="$ok" '
    /^tempo total:/      { total = $3 }
    /^faltas de pagina:/ { faltas = $4 }
    /^substituicoes:/    { subst = $2 }
    /^escritas de volta:/ { escr = $4 }
    /^pico de quadros:/  { pico = $4 }
    /^pid programa  .*faltas/ { tab = 1; next }
    tab && $2 == "calc"    { if ($3 ~ /vivo/) rcalc = "vivo"; else { if ($3 > rcalc) rcalc = $3 }; fcalc += $NF }
    tab && $2 == "paginas" { rpag = $3; fpag = $NF }
    /vezes em estado/    { tab = 0 }
    /^\[executor\] nucleo/ { nuc = $3; oc = $5; us = $7; gsub(",", "", nuc); gsub(",", "", oc) }
    /^\[executor\] maior trecho/ { trecho = $7 }
    END {
      pct = (nuc + oc + us) > 0 ? 100 * nuc / (nuc + oc + us) : 0
      if (rpag == "") rpag = "-"
      if (fpag == "") fpag = "-"
      printf "%-10s %-3s %6s %6s %5s %9s %6.1f%% %7s %5s %9s %9s %6s %6s\n", nome, ok, faltas, subst, escr, total, pct, trecho, pico, rcalc, rpag, fcalc, fpag
    }' resultados/$nome.txt >> resultados/resumo.txt
}

roda parte1 1 0 0
roda demanda 2 0 0
for s in 1 2; do
  [ $s = 1 ] && alg=fifo || alg=lru
  for l in $LIMITES; do
    roda $alg-l$l 2 $s $l
  done
done

echo
cat resultados/resumo.txt
