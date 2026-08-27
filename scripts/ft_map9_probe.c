#include <stdio.h>
#include <string.h>

/* 第9批映射候选实测: fopen/fgetc/fputc/fseek/ftell/rewind/feof/fflush (bn 内置, FILE*) */

int main(void) {
    /* fopen w + fputs + fclose 写文件 */
    FILE *f = fopen("ft_m9.txt", "w");
    if (!f) { printf("FAIL fopen w\n"); return 1; }
    fputs("hello", f);
    fclose(f);
    /* fopen r + fgetc 读回 */
    f = fopen("ft_m9.txt", "r");
    if (!f) { printf("FAIL fopen r\n"); return 1; }
    int c = fgetc(f);
    if (c != 'h') { printf("FAIL fgetc1: %c\n", c); return 1; }
    c = fgetc(f);
    if (c != 'e') { printf("FAIL fgetc2: %c\n", c); return 1; }
    /* feof 读到尾 */
    char buf[16]; int n = 0;
    while (n < 10) { c = fgetc(f); if (feof(f)) break; buf[n++] = c; }
    buf[n] = 0;
    if (strcmp(buf, "ll") != 0) { printf("FAIL rest: %s\n", buf); return 1; } /* qcc feof=位置比较: 读完 'o'(pos=5>=size=5) 即置位, buf 只含 "ll" */
    /* ftell 位置 */
    long pos = ftell(f);
    if (pos != 5) { printf("FAIL ftell: %ld\n", pos); return 1; }
    /* rewind 回到头 */
    rewind(f);
    c = fgetc(f);
    if (c != 'h') { printf("FAIL rewind: %c\n", c); return 1; }
    /* fseek 定位 */
    fseek(f, 2, 0); /* SEEK_SET=0 */
    c = fgetc(f);
    if (c != 'l') { printf("FAIL fseek: %c\n", c); return 1; }
    /* fputc + fflush */
    fclose(f);
    f = fopen("ft_m9.txt", "a");
    if (!f) { printf("FAIL fopen a\n"); return 1; }
    fputc('!', f);
    fflush(f);
    fclose(f);
    f = fopen("ft_m9.txt", "r");
    if (!f) { printf("FAIL fopen r2\n"); return 1; }
    fseek(f, 5, 0); /* SEEK_SET=0 */
    c = fgetc(f);
    if (c != '!') { printf("FAIL fflush: %c\n", c); return 1; }
    fclose(f);
    printf("PASS\n");
    return 0;
}
