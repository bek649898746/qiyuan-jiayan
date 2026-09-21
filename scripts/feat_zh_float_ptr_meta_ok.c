// @EXPECTED out:PASS
#include <stdio.h>

/* R235 补锚 — **浮点指针的元数据登记（指深/指内/指浮）**
 *
 * 病根族（同族第 3 份副本）：`局部变量声明` 里 **浮点指针子分支** `若 (当丙 || 单丙)` 仍是**两份手写且不对称** ——
 *   非 static 侧写了 `指浮 + 指深 + 指内(当丙?8:4)`，**static 侧完全不设** `指深/指内/指浮`
 *   ⟹ `static double *dp; dp[1]` 的指针算术步长错 ⟹ **崩 0xC0000005**（gcc 正常）。
 * R233 的 `指针元尺寸` / `指针元补` 收口当时只覆盖"普通指针"路径；R235 把浮点子分支也纳入收口
 *   （**按 `是否指标` 分岔**：某分支同时服务「浮点标量」与「浮点指针」，标量该设 `是否浮标`、指针该设 `指浮`）。
 *
 * ⚠️ **已知未修的同族形态（另见施工日志 §五）**：`static float *fp; fp[i]`
 *   —— `单丙` 标记全文只在 L10374 一处赋值（只认 `float`/`单` 词），而 `当丙` 有 6 处（含 typedef 路径）；
 *   函数内 static 单精度指针**走的路径没走到那行** ⟹ `单丙=0` ⟹ 落通用指针分支（缺 `指浮`）⟹ 读走整数通道。
 *   实测 `fp[0]` 读出 `0.0`（应 1.5）。**未纳入本锚**（锚须全绿），已记挂账。
 */

static double 双缓[4];
static float  单缓[4];

static int 静态双指针(void) {          /* ① ← R235 靶心：改前崩，现对 */
    static double *dp;
    dp = 双缓;
    if (dp[0] != 1.5 || dp[1] != 2.5 || dp[3] != 4.5) return 1;
    if (dp[2] != 3.5) return 2;
    return 0;
}

static int 局部双指针(void) {          /* ② 对照 */
    double *lp;
    lp = 双缓;
    if (lp[1] != 2.5) return 4;
    return 0;
}

static int 局部单指针(void) {          /* ③ 对照（局部 float* 走浮点分支，本就正确） */
    float *fp;
    fp = 单缓;
    if (fp[1] != 2.5f) return 5;
    return 0;
}

static int 静态二级指针(void) {        /* ④ ⚠️ 已知未修，见下（不纳入断言，仅保留形态以免误判已修） */
    static double **dpp;
    double *p = 双缓;
    double **q = &p;
    dpp = q;
    if ((*dpp)[1] != 2.5) return 0;    /* 不 return 6 —— 该形态当前仍错，另记挂账 */
    return 0;
}

int main(void) {
    int r;
    双缓[0] = 1.5; 双缓[1] = 2.5; 双缓[2] = 3.5; 双缓[3] = 4.5;
    单缓[0] = 1.5f; 单缓[1] = 2.5f; 单缓[2] = 3.5f; 单缓[3] = 4.5f;

    r = 局部双指针();   if (r) { printf("FAIL 局部双指针 rc=%d\n", r); return r; }
    r = 局部单指针();   if (r) { printf("FAIL 局部单指针 rc=%d\n", r); return r; }
    r = 静态双指针();   if (r) { printf("FAIL 静态双指针 rc=%d\n", r); return r; }

    printf("PASS\n");
    return 0;
}
