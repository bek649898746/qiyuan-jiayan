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
 * ✅ **已修（R236）**：`static float *fp; fp[i]` 原读出 `0.0`（应 1.5）—— 两处同族遗漏：
 *   ① **型词循环缺 `float`**（`局部变量声明` 的首词元判定有 `单丙`，但型词循环里只有 `double` 没有 `float`
 *      ⟹ `static` 打头的声明拿不到 `单丙` ⟹ 落通用指针分支）；② **静态浮点分支缺 `指单`**（只设了 `单标`）。
 *   R236 两刀修复；本锚的 **④ 静态单指针** 即该形态。
 * ⏳ **仍未修（不在本锚内，另记挂账）**：`static double **dpp; (*dpp)[1]` —— 静态**二级**指针四种读法全错
 *   （`dpp[0][0]` / `(*dpp)[0]` / `(*dpp)[1]` / `**dpp`），而**局部**二级 `q[0][1]` 正确。
 *   见 `_DEBT_LEDGER.md` R236 条目与施工日志 §四。
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

static int 静态单指针(void) {          /* ④ ← R236 修：型词循环缺 float ⟹ 单丙 恒 0 ⟹ 落通用分支（读出 0.0） */
    static float *fp;
    fp = 单缓;
    if (fp[0] != 1.5f) return 3;
    if (fp[2] != 3.5f) return 7;
    return 0;
}

int main(void) {
    int r;
    双缓[0] = 1.5; 双缓[1] = 2.5; 双缓[2] = 3.5; 双缓[3] = 4.5;
    单缓[0] = 1.5f; 单缓[1] = 2.5f; 单缓[2] = 3.5f; 单缓[3] = 4.5f;

    r = 局部双指针();   if (r) { printf("FAIL 局部双指针 rc=%d\n", r); return r; }
    r = 局部单指针();   if (r) { printf("FAIL 局部单指针 rc=%d\n", r); return r; }
    r = 静态双指针();   if (r) { printf("FAIL 静态双指针 rc=%d\n", r); return r; }
    r = 静态单指针();   if (r) { printf("FAIL 静态单指针 rc=%d\n", r); return r; }

    printf("PASS\n");
    return 0;
}
