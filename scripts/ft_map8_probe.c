#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 第8批映射候选实测: memset/memcpy/malloc/free/calloc/strcmp/strcpy (bn 内置) */

int main(void) {
    /* memset */
    char buf[16];
    memset(buf, 'A', 8); buf[8] = 0;
    if (strcmp(buf, "AAAAAAAA") != 0) { printf("FAIL memset: %s\n", buf); return 1; }
    /* memcpy */
    char src[8] = "hello";
    char dst[8];
    memcpy(dst, src, 6);
    if (strcmp(dst, "hello") != 0) { printf("FAIL memcpy: %s\n", dst); return 1; }
    /* malloc/free 写入 */
    int *p = malloc(4 * sizeof(int));
    if (!p) { printf("FAIL malloc\n"); return 1; }
    p[0] = 42; p[3] = 99;
    if (p[0] != 42 || p[3] != 99) { printf("FAIL malloc wr: %d %d\n", p[0], p[3]); return 1; }
    free(p);
    /* calloc 清零 */
    int *q = calloc(4, sizeof(int));
    if (!q) { printf("FAIL calloc\n"); return 1; }
    if (q[0] != 0 || q[3] != 0) { printf("FAIL calloc zero: %d %d\n", q[0], q[3]); return 1; }
    q[1] = 7;
    if (q[1] != 7) { printf("FAIL calloc wr\n"); return 1; }
    free(q);
    /* strcmp */
    if (strcmp("abc", "abc") != 0) { printf("FAIL strcmp eq\n"); return 1; }
    if (strcmp("abc", "abd") >= 0) { printf("FAIL strcmp lt\n"); return 1; }
    if (strcmp("abd", "abc") <= 0) { printf("FAIL strcmp gt\n"); return 1; }
    /* strcpy */
    char s2[8];
    strcpy(s2, "xyz");
    if (strcmp(s2, "xyz") != 0) { printf("FAIL strcpy: %s\n", s2); return 1; }
    printf("PASS\n");
    return 0;
}
