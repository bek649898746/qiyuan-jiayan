// @EXPECTED exit:0
// @EXPECTED out:PASS x=99
#include <stdio.h>

/* 甲言 重借用 t7 (2026-09-07): 可变借用 变量 传入函数 (&x = 重借用), 函数返回后原可变借用保留 */
int 加一(int *p) { return *p + 1; }

int 主(void) {
    int x = 10;
    可变借用 x;
    int r = 加一(&x);   /* 重借用: &x 传入函数, 原借用保留 (不误报 A3001) */
    if (r != 11) { printf("FAIL r: %d\n", r); return 1; }
    放还 x;
    x = 99;             /* 放还后可写 */
    printf("PASS x=%d\n", x);
    return 0;
}
