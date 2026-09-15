// @EXPECTED exit:0
// @EXPECTED out:PASS
#include <stdio.h>
/* 显式G (2026-09-07): 错误联合 0=成功 语义 — 3 层 尝试 传播 + 显式判断豁免 (比较即检查) */
错误 int f3(int x) { if (x < 0) 错 100; return 0; }
错误 int f2(int x) { 尝试 f3(x); return 0; }
错误 int f1(int x) { 尝试 f2(x); return 0; }
int main(void) {
    if (f1(-1) == 100) { /* 显式判断: 错误路径 (3 层传播 f3→f2→f1) */ }
    else { printf("FAIL err\n"); return 1; }
    if (f1(7) == 0) { /* 显式判断: 成功路径 */ }
    else { printf("FAIL ok\n"); return 2; }
    printf("PASS\n");
    return 0;
}
