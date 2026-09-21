// @EXPECTED out:PASS
#include <stdio.h>

/* R244 (2026-09-22) 补锚 — **多级指针 解引用链 的 浮性传递**
 *
 * 病根（两处结构性缺陷 + 一处同族漏改，详见 scratch_gl/_r244_fix.py 头注释与施工日志）：
 *   ① `节点浮性` ② 缺「解到底」守卫 ⟹ `*pp`（pp 指深 ≥ 2）的结果**仍是指针**，
 *      却被判成浮点标量 ⟹ 发射 `浮取 xmm0,[r0]`（movsd）—— **只写 xmm0、不动 r0**；
 *   ② `节点浮性` 对**链式操作数**（操作数是解引用节点）四条判据全落空 ⟹ `**pp` 判「非浮」
 *      ⟹ 走整数通道加载，与内层遗留的 xmm0 错配 ⟹ `%f` 读到「地址当 double」≈ `0.000000`；
 *   ③ 函数内 static 浮点指针 缺 `变量基登写`（A22 声明期基类码）⟹ ② 的修复对它不生效。
 *
 * 实测（修复前，`scratch_gl/r9/r244b.c`）：
 *   整数二级四种读法 **全对**（`0x11223344` 逐值正确）⟹ **加载宽度无损**，纯「浮点通道」丢失；
 *   浮点二级在 **函数内 static / 局部 全 0.000000**，**文件级恰好幸免**（其 `变量指浮 == 0`）。
 * 本锚把三种存储类别 × 三种读法做成矩阵，任一格退化即红。 */

static double 双缓[4];
static double *全一;
static double **全二;

static int 静态二级(void) {            /* 函数内 static：② 的靶心（同族漏改 ③ 在此暴露） */
    static double *p;
    static double **dpp;
    p = 双缓;
    dpp = &p;
    if (**dpp != 1.5) return 1;
    if ((*dpp)[1] != 2.5) return 2;
    if (dpp[0][2] != 3.5) return 3;
    return 0;
}

static int 静态二级间接(void) {        /* 同上，但经中间局部变量赋值（`dpp = q;`） */
    static double *p;
    static double **dpp;
    double **q;
    p = 双缓;
    q = &p;
    dpp = q;
    if (**dpp != 1.5) return 8;
    if ((*dpp)[3] != 4.5) return 9;
    return 0;
}

static int 文件级二级(void) {          /* 文件级 static：修复前 恰好 幸免 —— 看护 不得 退化 */
    全一 = 双缓;
    全二 = &全一;
    if (**全二 != 1.5) return 4;
    if ((*全二)[3] != 4.5) return 5;
    return 0;
}

static int 局部二级(void) {            /* 局部（栈）：修复前 同样全错 */
    double *p;
    double **q;
    p = 双缓;
    q = &p;
    if (**q != 1.5) return 6;
    if ((*q)[1] != 2.5) return 7;
    return 0;
}

static int 一级对照(void) {            /* 一级指针（本就正确）：防「修过头」 */
    double *p;
    static double *sp;
    p = 双缓;
    sp = 双缓;
    if (p[1] != 2.5) return 10;
    if (sp[2] != 3.5) return 11;
    return 0;
}

int main(void) {
    int r;
    双缓[0] = 1.5; 双缓[1] = 2.5; 双缓[2] = 3.5; 双缓[3] = 4.5;

    r = 一级对照();     if (r) { printf("FAIL 一级对照 rc=%d\n", r); return r; }
    r = 局部二级();     if (r) { printf("FAIL 局部二级 rc=%d\n", r); return r; }
    r = 静态二级();     if (r) { printf("FAIL 静态二级 rc=%d\n", r); return r; }
    r = 静态二级间接(); if (r) { printf("FAIL 静态二级间接 rc=%d\n", r); return r; }
    r = 文件级二级();   if (r) { printf("FAIL 文件级二级 rc=%d\n", r); return r; }

    printf("PASS\n");
    return 0;
}
