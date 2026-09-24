/* C2 锚点 (2026-09-24 启元 · 第 282 站): 尺寸(*X) 对 多级 指针 按 **剩余 指针 层级** 取 尺寸.
   病灶: `尺寸(*X)` 的 `乘` 分支 原 只 在 解引用 层数 >= 2 时 取「最深层 基类型 大小」,
   而 解 一层 后 **仍 是 指针** (指深 >= 2) 时 落 默认支 `变量指元素` = 最内层 标量 尺寸
   ==> 签名 收敛 为「最内层标量」: 单精度**==>4 · 整数**==>4 · 字符**==>1 · 双精度**==>8(掩盖).
   危害: `分配内存(n * 尺寸(*p))` 分配不足 ==> 堆溢出炸弹.
   局部 与 文件级 都受影响; 一级 指针 与 指针数组 本就正确. */
#include <stdio.h>
static float fbuf[4] = { 1.5f, 2.5f, 3.5f, 4.5f };
static float **gq;
int main(void) {
    float **k;
    float ***k3;
    int **ip;
    char **cp;
    double **dp;
    float *k1;
    float *ka[2];
    printf("A=%d B=%d C=%d D=%d\n", (int)sizeof(*k), (int)sizeof(*k3), (int)sizeof(*ip), (int)sizeof(*cp));
    printf("E=%d F=%d G=%d H=%d\n", (int)sizeof(*dp), (int)sizeof(*k1), (int)sizeof(*ka), (int)sizeof(*gq));
    printf("I=%d J=%d\n", (int)sizeof(k), (int)sizeof(ka));
    return 0;
}
// @EXPECTED out:A=8 B=8 C=8 D=8
// @EXPECTED out:E=8 F=4 G=8 H=8
// @EXPECTED out:I=8 J=16
// @EXPECTED exit:0
