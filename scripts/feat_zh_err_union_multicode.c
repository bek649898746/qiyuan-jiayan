// @EXPECTED exit:0
// @EXPECTED out:PASS
#include <stdio.h>
/* 攻坚 2026-09-07: 多错误码 全码传播 — 错误函数含 错 100 + 错 200; 尝试 须传播首码 100
   (原 单 返错码 只记最后 错 200 → 首码 100 路径漏传播当成功值 → g(-1) 误返 7) */
错误 int f(int x) { if (x < 0) 错 100; if (x > 100) 错 200; return x; }
错误 int g(int x) { 尝试 f(x); return 7; }
int main(void) {
    if (g(-1) == 100) { } else { printf("FAIL first-code\n"); return 1; }
    if (g(200) == 200) { } else { printf("FAIL last-code\n"); return 2; }
    if (g(5) == 7) { } else { printf("FAIL ok\n"); return 3; }
    printf("PASS\n");
    return 0;
}
