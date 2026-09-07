// @EXPECTED exit:0
#include <stdio.h>
/* 显式G 载荷化 (2026-09-07): 尝试 常量错误码精确传播 — 非零成功值 不误传播 */
错误 int f(int x) { if (x < 0) 错 100; return x * 2; }
错误 int g(int x) { 尝试 f(x); return 7; }
int main(void) {
    if (g(5) == 7) { /* f(5)=10 非零成功 → 尝试 不误传播 → g 返 7 */ }
    else { printf("FAIL ok\n"); return 1; }
    if (g(-1) == 100) { /* f(-1)=100 错误 → 尝试 传播 → g 返 100 */ }
    else { printf("FAIL err\n"); return 2; }
    printf("PASS\n");
    return 0;
}
