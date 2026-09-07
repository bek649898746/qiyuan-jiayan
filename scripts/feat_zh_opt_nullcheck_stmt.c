// @EXPECTED exit:0
#include <stdio.h>
/* 显式H2 (2026-09-07): 判空 x; 语句 → 窄化 (清可空, 断言非空) — 窄化后解引用放行 */
int main(void) {
    可空 int *p = 0;
    int real = 5;
    p = &real;
    判空 p;          /* 断言非空, 窄化 */
    int v = *p;      /* 窄化后解引用放行 */
    if (v != 5) { printf("FAIL: %d\n", v); return 1; }
    printf("PASS\n");
    return 0;
}
