#!/bin/sh
# confere_disco.sh -- confere o disco gerado por gera_disco.sh.
#
# uso: confere_disco.sh dir prog0 prog1 ...
#
# Relê dir/programas_disco.h e confere, para cada programa:
#   - a imagem começa com a capa: "jmp 8", no endereço 4 o tamanho da
#     própria imagem e no 6 o início dos dados;
#   - os dados começam numa página nova; as páginas de código cobrem até
#     lá e as de dados vão dali até o fim;
#   - as marcas de página que começa no meio de instrução são as que o
#     cruza_paginas calcula, uma por página de código;
#   - os bytes do disco, a partir do setor indicado, são os da imagem, e o
#     resto do último setor é zero;
#   - os programas ficam em sequência, sem sobreposição, e o disco não tem
#     nada além deles;
# e que o mcc aceita o programas_disco.h (o núcleo vai incluí-lo).

DIR=$1
shift
DISCO=$DIR/disco.img
INDICE=$DIR/programas_disco.h
TAM_SETOR=512
MCC=../compilador_c/bin/mcc
CRUZA=${CRUZA:-bin/cruza_paginas}
falhas=0

falha() { echo "  FALHA: $*"; falhas=$((falhas + 1)); }

# i-ésimo valor (a partir de 0) do vetor "nome" no índice
valor() {
    sed -n "s/^.*[ *]$1\[[0-9]*\] = { \(.*\) };$/\1/p" "$INDICE" |
        tr -d ' "' | cut -d, -f$(($2 + 1))
}

[ -f "$DISCO" ] && [ -f "$INDICE" ] || { echo "falta $DISCO ou $INDICE"; exit 1; }

esperado=0
i=0
for prog in "$@"; do
    bin=$DIR/$prog.bin
    setor=$(valor prog_setor $i)
    pag=$(valor prog_paginas $i)
    bytes=$(valor prog_bytes $i)
    nome=$(valor prog_nome $i)

    [ "$nome" = "$prog" ] || falha "posição $i: índice diz '$nome', esperado '$prog'"

    capa=$(od -An -tx1 -N4 "$bin" | tr -d ' \n')
    [ "$capa" = "01e00008" ] || falha "$prog: não começa com 'jmp 8' ($capa)"

    tam_capa=$(od -An -tu1 -j4 -N2 "$bin" | awk 'NF { print $1 * 256 + $2; exit }')
    tam_bin=$(wc -c < "$bin" | tr -d ' ')
    [ "$tam_capa" = "$tam_bin" ] || falha "$prog: capa diz $tam_capa bytes, imagem tem $tam_bin"
    [ "$bytes" = "$tam_bin" ] || falha "$prog: índice diz $bytes bytes, imagem tem $tam_bin"
    [ "$pag" = $(( (tam_bin + TAM_SETOR - 1) / TAM_SETOR )) ] || falha "$prog: $pag páginas não cobre $tam_bin bytes"

    ini=$(od -An -tu1 -j6 -N2 "$bin" | awk 'NF { print $1 * 256 + $2; exit }')
    [ $((ini % TAM_SETOR)) -eq 0 ] || falha "$prog: dados em $ini, fora do início de uma página"
    cod=$(valor prog_codigo $i)
    dini=$(valor prog_dados_ini $i)
    dad=$(valor prog_dados $i)
    [ $((cod * TAM_SETOR)) -ge "$ini" ] && [ $(((cod - 1) * TAM_SETOR)) -lt "$ini" ] ||
        falha "$prog: $cod páginas de código, mas os dados começam em $ini"
    if [ "$ini" -lt "$tam_bin" ]; then
        [ "$dini" = $((ini / TAM_SETOR)) ] && [ $((dini + dad)) = "$pag" ] ||
            falha "$prog: dados nas páginas $dini+$dad, mas vão de $ini a $tam_bin"
    else
        [ "$dad" = 0 ] || falha "$prog: sem dados, mas o índice diz $dad páginas"
    fi

    cini=$(valor prog_cruza_ini $i)
    marcas=""
    for p in $(seq 0 $((cod - 1))); do
        marcas="$marcas $(valor pagina_cruza $((cini + p)))"
    done
    [ "$(echo $marcas)" = "$("$CRUZA" "$bin")" ] || falha "$prog: marcas de cruzamento erradas ($marcas)"

    [ "$setor" = "$esperado" ] || falha "$prog: começa no setor $setor, esperado $esperado (buraco ou sobreposição)"

    # conteúdo no disco = imagem; resto do último setor = zeros
    dd if="$DISCO" bs=$TAM_SETOR skip="$setor" count="$pag" 2>/dev/null > "$DIR/.confere_$prog"
    head -c "$tam_bin" "$DIR/.confere_$prog" | cmp -s - "$bin" || falha "$prog: bytes no disco diferem da imagem"
    nao_zero=$(tail -c +$((tam_bin + 1)) "$DIR/.confere_$prog" | od -v -An -tu1 | tr -s ' ' '\n' | grep -cv '^0*$')
    [ "$nao_zero" = 0 ] || falha "$prog: lixo depois da imagem no último setor"
    rm -f "$DIR/.confere_$prog"

    printf '  ok  %-8s setores %4d-%-4d (%d bytes)\n' "$prog" "$setor" $((setor + pag - 1)) "$tam_bin"
    esperado=$((setor + pag))
    i=$((i + 1))
done

tam_disco=$(wc -c < "$DISCO" | tr -d ' ')
[ "$tam_disco" = $((esperado * TAM_SETOR)) ] || falha "disco tem $tam_disco bytes, esperado $((esperado * TAM_SETOR))"

# o núcleo vai incluir o índice: o mcc precisa aceitá-lo
printf '#include "programas_disco.h"\nint main(void) { return prog_setor[0] + prog_paginas[0] + prog_bytes[0] + prog_codigo[0] + prog_dados_ini[0] + prog_dados[0] + prog_cruza_ini[0] + pagina_cruza[0] + prog_nome[0][0]; }\n' > "$DIR/.confere_indice.c"
"$MCC" -I"$DIR" "$DIR/.confere_indice.c" -o "$DIR/.confere_indice.asm" > /dev/null 2>&1 || falha "o mcc não compila programas_disco.h"
rm -f "$DIR/.confere_indice.c" "$DIR/.confere_indice.asm"

if [ $falhas -eq 0 ]; then
    echo "confere-disco: tudo certo ($i programas, $esperado setores)"
else
    echo "confere-disco: $falhas falha(s)"
    exit 1
fi
