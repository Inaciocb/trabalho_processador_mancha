#!/bin/sh
# gera_disco.sh -- monta o disco simulado com os programas de usuário e o
# índice que o núcleo usa para achá-los.
#
# uso: gera_disco.sh so_chamadas.h dir prog0 prog1 ...
#
# Para cada programa, lê dir/<prog>.full (imagem crua gerada pelo mob2bin,
# a partir do endereço 0) e escreve:
#   dir/<prog>.bin           a imagem cortada no tamanho certo
#   dir/disco.img            os programas em sequência, cada um começando
#                            num setor novo (1 setor = 512 bytes = 1 página)
#   dir/programas_disco.h    setor inicial, páginas, bytes e nome de cada um
#
# O tamanho de cada imagem vem da própria imagem: a palavra no endereço 4
# (ver usuario/entrada.asm), em big-endian como a memória do Mancha.
#
# Os programas têm que vir na ordem dos PROG_* de so_chamadas.h: o número
# que SO_CRIA_PROC recebe é o índice no disco. O script confere isso.

set -e

TAM_SETOR=512
MAX_SETORES=1440        # 2 faces x 40 trilhas x 18 setores (dispositivos.h)

if [ $# -lt 3 ]; then
    echo "uso: $0 so_chamadas.h dir prog..." >&2
    exit 1
fi
CHAMADAS=$1
DIR=$2
shift 2

erro() { echo "gera_disco: $*" >&2; exit 1; }

# valor de um #define em so_chamadas.h (vazio se não existir)
define() { awk -v n="$1" '$1 == "#define" && $2 == n { print $3 }' "$CHAMADAS"; }

n_esperado=$(define N_PROGRAMAS)
[ "$#" = "$n_esperado" ] || erro "$# programas, mas N_PROGRAMAS = $n_esperado em $CHAMADAS"

DISCO=$DIR/disco.img
INDICE=$DIR/programas_disco.h
: > "$DISCO"

setores=""
paginas=""
bytes=""
nomes=""
setor=0
i=0
for prog in "$@"; do
    nome_maiusc=$(echo "$prog" | tr 'a-z' 'A-Z')
    [ "$(define "PROG_$nome_maiusc")" = "$i" ] || erro "$prog está na posição $i, mas PROG_$nome_maiusc não é $i"

    full=$DIR/$prog.full
    [ -f "$full" ] || erro "falta $full"
    tam=$(od -An -tu1 -j4 -N2 "$full" | awk 'NF { print $1 * 256 + $2; exit }')
    [ "$tam" -gt 6 ] && [ "$tam" -le 65536 ] || erro "$prog: tamanho inválido na capa ($tam)"

    head -c "$tam" "$full" > "$DIR/$prog.bin"
    pag=$(( (tam + TAM_SETOR - 1) / TAM_SETOR ))
    [ $((setor + pag)) -le $MAX_SETORES ] || erro "não cabe no disco: $prog"

    # o programa e zeros até completar o último setor
    cat "$DIR/$prog.bin" >> "$DISCO"
    resto=$(( pag * TAM_SETOR - tam ))
    [ "$resto" -eq 0 ] || head -c "$resto" /dev/zero >> "$DISCO"

    printf '  %-8s setor %4d  %2d paginas  %5d bytes\n' "$prog" "$setor" "$pag" "$tam"

    sep=""; [ $i -eq 0 ] || sep=", "
    setores="$setores$sep$setor"
    paginas="$paginas$sep$pag"
    bytes="$bytes$sep$tam"
    nomes="$nomes$sep\"$prog\""
    setor=$((setor + pag))
    i=$((i + 1))
done

cat > "$INDICE" <<EOF
// programas_disco.h -- gerado por ferramentas/gera_disco.sh; não editar.
// Programa i (o número de SO_CRIA_PROC, ver so_chamadas.h) ocupa
// prog_paginas[i] setores do disco a partir do setor linear prog_setor[i]
// (setor linear = (face * 40 + trilha) * 18 + setor).

#define DISCO_N_PROGRAMAS $i

int prog_setor[$i] = { $setores };
int prog_paginas[$i] = { $paginas };
int prog_bytes[$i] = { $bytes };
char *prog_nome[$i] = { $nomes };
EOF

echo "  disco: $setor setores usados de $MAX_SETORES"
