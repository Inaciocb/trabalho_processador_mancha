// eterno.c -- nunca termina: o init o mata (como no T2).

#include "so.h"

int main(void)
{
    int x;
    x = 0;
    for (;;) {
        x = x + 1;
    }
    return 0;
}
