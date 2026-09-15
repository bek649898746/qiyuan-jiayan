// @EXPECTED exit:0
// @EXPECTED out:PASS
#include <stdio.h>
/* 真 sum type ABI (2026-09-07): 运行时判别通道 — 成功载荷数值 == 声明错误码 不再误传播
   f 成功返 100 (payload) 恰等于 错 100 → 旧 0=成功 语义 (编译期码比较) 误判为错误;
   新: 错误函数打包 {tag|码/值}, 尝试 按 tag 判定 → g(5) 成功路径返 7 */
错误 int f(int x) { if (x < 0) 错 100; return 100; }
错误 int g(int x) { 尝试 f(x); return 7; }
int main(void) {
    if (g(5) == 7) { } else { printf("FAIL ok\n"); return 1; }
    if (g(-1) == 100) { } else { printf("FAIL err\n"); return 2; }
    printf("PASS\n");
    return 0;
}
