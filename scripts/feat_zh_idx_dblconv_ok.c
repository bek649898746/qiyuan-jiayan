/* C8 锚点 (2026-09-24 启元 · 第 277 站): 整数指针下标 的 值 进 浮点上下文.
   病灶 = 节点浮性 ① 缓存 未 排除 节点_下标 (只 排了 节点_解引用).
   修复前: (双精度)q[0] / (双精度)q[1] / (双精度)q[0]+0.5 / 双精度 d=(双精度)q[2] 全 静默 得 0. */
#include <stdio.h>
static int mm[3] = { 11, 22, 33 };
static double f0(int *q) { return (double)q[0]; }
static double f1(int *q) { return (double)q[1]; }
static double f2(int *q) { return (double)q[0] + 0.5; }
static double f3(int *q) { double d = (double)q[2]; return d; }
int main(void) {
    printf("A=%.0f B=%.0f C=%.1f D=%.0f\n", f0(mm), f1(mm), f2(mm), f3(mm));
    return 0;
}
// @EXPECTED out:A=11 B=22 C=11.5 D=33
// @EXPECTED exit:0
