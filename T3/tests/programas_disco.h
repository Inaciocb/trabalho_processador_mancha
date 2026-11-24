// programas_disco.h falso para os testes nativos (o de verdade é gerado
// pelo mkdisco em build/): todo programa tem 4 páginas de código e 1 de
// dados, 5 setores a partir de prog*5, e uma instrução que cruza para a
// página 2.

#define N_PROGRAMAS_DISCO 11
#define SETOR_TROCA 100

int prog_setor[11] = {0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50};
int prog_n_ro[11] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};
int prog_n_rw[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
int prog_cruza_ini[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int prog_cruza[2] = {2, -1};

char *programa_nome(int prog)
{
    (void)prog;
    return "teste";
}
