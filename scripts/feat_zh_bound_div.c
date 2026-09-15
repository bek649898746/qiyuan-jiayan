// @EXPECTED exit:0
// @EXPECTED out:PASS
#include <stdio.h>
/* 显式F2 (2026-09-07): 运行时边界 — 非零除数 正常 */
int main(void) {
    int a = 10, b = 2;
    int c = a / b;
    int d = 10 / 2;
    int e = a % 3;
    if (c != 5 || d != 5 || e != 1) { printf("FAIL\n"); return 1; }
    printf("PASS\n");
    return 0;
}
