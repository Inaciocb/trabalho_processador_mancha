// Testes nativos do gerenciador de quadros.
#include <assert.h>
#include <stdio.h>
#include "../src/mem.c"

static int panicos;

void k_puts(char *s) { (void)s; }
void k_int(int v) { (void)v; }
void para(void) { panicos++; }

static void aloca_tudo_e_libera(int primeiro, int ultimo)
{
    int visto[MAX_QUADROS] = {0};
    int total = ultimo - primeiro + 1;
    int q, n = 0;

    mem_inicia(primeiro, ultimo);
    assert(quadros_livres() == total);
    while ((q = aloca_quadro()) >= 0) {
        assert(q >= primeiro && q <= ultimo);
        assert(!visto[q]);
        visto[q] = 1;
        n++;
        assert(quadros_livres() == total - n);
    }
    assert(n == total);
    assert(aloca_quadro() == -1);

    for (q = primeiro; q <= ultimo; q++) libera_quadro(q);
    assert(quadros_livres() == total);
    assert(panicos == 0);
}

static void reaproveita(void)
{
    int a, b, c;

    mem_inicia(10, 12);
    a = aloca_quadro();
    b = aloca_quadro();
    libera_quadro(a);
    c = aloca_quadro();
    assert(c == a);
    assert(b != a);
    assert(quadros_livres() == 1);
}

static void erros_dao_panico(void)
{
    mem_inicia(10, 12);
    panicos = 0;
    libera_quadro(11);
    assert(panicos == 1);
    libera_quadro(9);
    assert(panicos == 2);
    libera_quadro(13);
    assert(panicos == 3);
    assert(quadros_livres() == 3);
    panicos = 0;
}

int main(void)
{
    aloca_tudo_e_libera(56, QUADRO_LIMITE - 1);
    aloca_tudo_e_libera(0, 0);
    reaproveita();
    erros_dao_panico();
    printf("tests/mem: ok\n");
    return 0;
}
