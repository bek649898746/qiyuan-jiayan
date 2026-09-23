/* R255 看护锚（第二枚）—— 「**各种解引用根** 上的 `(双精度)*X`」 的加载通道
   ---------------------------------------------------------------------------
   与 `feat_zh_deref_cast_double_ok.c` 同一根因（R255 刀2，不动点 B9EB4BE0 → 8E3F3924）:
     `(双精度)X` 转型把 `节浮标` 打在操作数（解引用节点）自身上 ⟹ `节点浮性` ① 缓存命中
     ⟹ `例 节点_解引用` 改发 `movsd`（只写 xmm0、rax 不动 = 指针值）
     ⟹ 随后的整数提升 `cvtsi2sd xmm0, rax` 用指针值覆盖。修 = `节点浮性` ① 缓存遵从源类型。

   ⚠️ 为什么需要**第二枚**锚: 旧编译器（B9EB4BE0）在本锚 **7 项里错了 6 项**，
      而闸原有的 804 项 + 判据 C 的 582 个样本 **一个都没覆盖**这些解引用根形态
      （否则旧编译器上闸就会红）⟹ 属于**整族长期未保护面**。

   覆盖的解引用根: 函数返回指针 / 结构体成员指针 / 数组元素基 / 二级 / 三级 /
                   指针数组元素 / 括号法（唯一在旧编译器上恰好正确的对照项）
   一行输出（闸的 `@EXPECTED out:` 只保留最后一条）。 */
// @EXPECTED exit:0
// @EXPECTED out:DCC 11.000000 11.000000 22.000000 11.000000 11.000000 22.000000 22.000000
#include <stdio.h>

static long long  lbuf[4];
static long long *parr[4];

struct S { long long *p; };
static struct S s;

static long long *getp(void) { return lbuf; }

int main(void) {
    long long  **pp;
    long long ***ppp;
    lbuf[0] = 11; lbuf[1] = 22;
    parr[0] = lbuf; parr[1] = lbuf + 1;
    s.p = lbuf;
    pp = parr; ppp = &pp;
    printf("DCC %f %f %f %f %f %f %f\n",
           (double)*getp(),      /* 1 函数返回指针 解引用   : 11 （旧: 0.000000）*/
           (double)*s.p,         /* 2 结构体成员指针 解引用 : 11 （旧: 指针值）*/
           (double)*parr[1],     /* 3 数组元素基 解引用     : 22 （旧: 指针值+8）*/
           (double)**pp,         /* 4 二级解引用            : 11 （旧: 指针值）*/
           (double)***ppp,       /* 5 三级解引用            : 11 （旧: 指针值）*/
           (double)*pp[1],       /* 6 指针数组元素解引用    : 22 （旧: 指针值+8）*/
           (double)(*pp)[1]);    /* 7 括号法                : 22 （旧也正确 — 对照）*/
    return 0;
}
