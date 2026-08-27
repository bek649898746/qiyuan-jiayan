#include <stdio.h>

int main(void) {
    FILE *f = fopen("ft_m9.txt", "r");
    if (!f) { printf("FAIL open\n"); return 1; }
    printf("ftell0=%ld\n", ftell(f));
    int i, c;
    for (i = 0; i < 5; i++) c = fgetc(f);
    printf("ftell5=%ld\n", ftell(f));
    fclose(f);
    printf("DONE\n");
    return 0;
}
