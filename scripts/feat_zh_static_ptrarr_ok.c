/* C6 锚 (2026-09-24 启元 · 第 268 站): **函数内 static 指针数组 补 指内**.
   病灶: `例 节点_下标` 的 static 数组分支 (L10744) 只设 `指元素尺寸`, **漏设 `指内`**;
   非 static 局部数组由 L10769 (R255 刀5) 设了 ⟹ 两分支不对称。
   后果 (QCC_DUMP=vars 铁证): 全局 `static int *gp[2]` 指内=4, 而 **函数内** `static int *parr[2]`
     指内=**0** ⟹ (a) 表达式内重/长卯 回退默认 (b) **A9 刀的「指针到数组」判据 `变量指内查 > 0` 不命中**。
   修: 镜像 L10769 判据并补 `当丙` 分派 (⟹ `double *p[2]` 得 8)。
   本锚覆盖: 全局 vs 函数内 static 的 int 指针数组元素内索引 (`parr[1][1]` 旧版得 0, 应 4) +
     double 指针数组的 `指内` 登记 (经 A9 判据链间接生效) + 邻接检查。
   ⚠️ 本锚**不含** `double *dp[2]` 的二级索引 `dp[1][0]` —— 那是**独立缺陷 (C7)**:
     指针数组 + 浮点元素的二级下标少一次解引用, 非 static 同样错, 已另立挂账。 */
// @EXPECTED out:G0=10 G1=30 P0=1 P1=3 P1b=4 D0=1.50
// @EXPECTED exit:0
#include <stdio.h>

static int g[4] = {10,20,30,40};
static int *gp[2] = { g, g + 2 };

int main(void) {
    static int a[4] = {1,2,3,4};
    static int *parr[2] = { a, a + 2 };
    static double d[2] = {1.5, 2.5};
    static double *dparr[2] = { d, d + 1 };

    printf("G0=%d ", gp[0][0]);
    printf("G1=%d ", gp[1][0]);
    printf("P0=%d ", parr[0][0]);
    printf("P1=%d ", parr[1][0]);
    printf("P1b=%d ", parr[1][1]);
    printf("D0=%.2f", dparr[0][0]);   /* 只测 [0][0] (不涉 C7) */
    return 0;
}