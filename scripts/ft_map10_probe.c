#include <stdio.h>
#include <ctype.h>

/* 第10批映射候选实测: printf/fprintf/ferror/isalnum/scanf (bn 内置) */

int main(void) {
    /* printf */
    printf("P%d\n", 1);
    /* fprintf */
    FILE *f = fopen("fm10.txt", "w");
    if (!f) { printf("FAIL fopen\n"); return 1; }
    fprintf(f, "x=%d\n", 42);
    fclose(f);
    /* ferror: 句柄语义无错误跟踪 → 恒 0 (qcc 设计) */
    if (ferror(f) != 0) { printf("FAIL ferror\n"); return 1; }
    fclose(f);
    /* isalnum */
    if (!isalnum('a') || !isalnum('5')) { printf("FAIL isalnum1\n"); return 1; }
    if (isalnum('!')) { printf("FAIL isalnum2\n"); return 1; }
    if (isalnum('Z') || isalnum('0')) { if (!isalnum('Z') || !isalnum('0')) { printf("FAIL isalnum3\n"); return 1; } }
    /* scanf: 从标准输入读 */
    int v = 0;
    if (scanf("%d", &v) != 1) { printf("FAIL scanf\n"); return 1; }
    if (v != 42) { printf("FAIL scanf val: %d\n", v); return 1; }
    printf("PASS\n");
    return 0;
}
