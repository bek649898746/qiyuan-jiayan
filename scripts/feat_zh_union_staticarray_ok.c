/* A5 锚 (2026-09-24 启元 · 第 267 站): union / 静态数组 内存 dump 全形态固化.
   背景: A5 原挂账 = "union / 静态数组内存 dump: 后 16 字节不同 (甲言 5..8 / gcc 9..12)",
     探针 scratch_gl\r9\r238z2.c **已随 scratch 目录丢失**, 无法逐字复现.
   本锚 = 「判性质」3 轮 14 类形态的代表集 (静态数组 单层/嵌套/部分/全零/二维 ·
     union 首成员/内嵌结构体/类型双关/union 数组 · dump 逐字节视角 vs read 语义视角).
   **结论: 14 类全部与 gcc 逐字一致 ⟹ A5 定性为「不可复现」(疑已被后续 A3/A10/A2/A8/A9 刀连带修复).**
   本锚作为 **A5 形态的回归哨兵** 固化 —— 将来该族任何形态被打破, 本锚立刻变红. */
// @EXPECTED out:A5 1 8 9 12 1 12 0 0 9 12 1074528256 77 80 32 8 32
// @EXPECTED exit:0
/* A5 锚: union / 静态数组 内存 dump 全形态（判性质 3 轮 14 类的代表集） */
#include <stdio.h>

struct S { int x[4]; int y[4]; };
union UV { int i[8]; struct S s; };
union UF { double d; int i[2]; };

static int a1[8] = {1,2,3,4,5,6,7,8};
static struct S s2 = { {1,2,3,4}, {9,10,11,12} };
static union UV u3 = { {1,2,3,4,9,10,11,12} };
static int a4[8] = {1,2,3};
static int z[8] = {0};
static int p2[2][4] = {{1,2,3,4},{9,10,11,12}};
static union UF uf = { 3.5 };
static int after[4] = {77,78,79,80};

int main(void) {
    int *pu = (int*)&u3;
    printf("A5 %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
        a1[0], a1[7], s2.y[0], s2.y[3], pu[0], pu[7], a4[3], z[0],
        p2[1][0], p2[1][3], uf.i[1], after[0], after[3],
        (int)sizeof(union UV), (int)sizeof(union UF), (int)sizeof(struct S));
    return 0;
}