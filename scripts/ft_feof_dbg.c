#include <stdio.h>

int main(void) {
    FILE *f = fopen("ft_m9.txt", "r");
    if (!f) { printf("FAIL open\n"); return 1; }
    /* 读完 5 字节 */
    int i, c;
    for (i = 0; i < 5; i++) { c = fgetc(f); printf("c%d=%c(%d)\n", i, c >= 32 && c < 127 ? c : '?', c); }
    printf("feof_after5=%d\n", feof(f));
    c = fgetc(f);
    printf("c5=%d feof_after6=%d\n", c, feof(f));
    fclose(f);
    /* 直接 fseek 到 5 */
    f = fopen("ft_m9.txt", "r");
    fseek(f, 5, 0);
    printf("feof_at5=%d\n", feof(f));
    fclose(f);
    printf("DONE\n");
    return 0;
}
