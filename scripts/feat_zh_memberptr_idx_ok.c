/* feat_zh_memberptr_idx_ok.c — 成员指针字段作数组基 的「写」 (A10) 根治 2026-09-23 启元

   病灶（A10，在 EA445B4A 上必红）：
     形态 `bx.p[1] = v`（**成员基** + 指针字段 + 下标 **写**）—— 箭头基同形 `pb->p[1] = v` 正确，
     `(*bx.pp)[1] = v`（显式解引用）正确 ⟹ 只有「成员基 + 隐式解引用」这一格错。

     字节级铁证（双探针只差一行 + objdump）：
       成员基 ❌  `mov $0x8,%r9d; imul %r9d,%r11d`（偏移 1×**8**）+ `movsd %xmm0,(%rax)`（**8 字节**存）
       箭头基 ✅  `mov $0x4,%r9d; imul %r9d,%r11d`（偏移 1×**4**）+ `cvtsd2ss`+`movss`（**4 字节**存）
     ⟹ 写落到 `fbuf[2]` 而非 `fbuf[1]`（读回仍是原值 2.50，表现为「写不生效」）。

     根因（插桩链 `QCC_DBG_A10`，5 轮）：`剩未(构序, 字段)` 按「结构体**数组字段**」语义
     用 `构域行` = **指针自身尺寸 (8)** 布第一维，而指针字段的第一维下标是**解引用**（非行偏移）
     ⟹ 缩放应用 `字段指元素` (4)。下方既有的修正 `上薄[0] = 字段指元素` 被其自身守卫
     `!正五` 跳过（求写左值地址时 `正五 = 1`）。
     修：`剩未()` 内对**指针字段**（`字段是否指`）取 `字段指元素` 作 行/元素 宽（数组字段逐字不变）。

   本锚覆盖：静态结构体 / 全局结构体 / 局部结构体 × float* / double* / float** + 写后指针完整性。
   灵敏度：在 EA445B4A 上 W1/W2/W4 全红（写不生效）。 */
// @EXPECTED out:W1=8.50 W2=9.50 W3=7.50 W4=6.50 W5=5.50 R1=5.50 ok
// @EXPECTED exit:0
#include <stdio.h>

struct box { float *p; float **pp; double *dp; };

static float  fbuf[4] = { 1.5f, 2.5f, 3.5f, 4.5f };
static double dbuf[4] = { 1.25, 2.25, 3.25, 4.25 };
static float *fp = fbuf;
static float **fpp = &fp;

static struct box  bx;                 /* 静态结构体（走「静态成员读取」分支） */
static struct box  gx;                 /* 全局结构体 */
static struct box *px = &bx;

static float *keep_p;                  /* 写后指针完整性哨兵 */

int main(void) {
    bx.p = fbuf;  bx.pp = &fp;  bx.dp = dbuf;
    gx.p = fbuf;  gx.dp = dbuf;
    keep_p = bx.p;

    fbuf[1] = 2.5f;  bx.p[1] = 8.5f;
    printf("W1=%.2f ", fbuf[1]);

    dbuf[1] = 2.25;  bx.dp[1] = 9.5;
    printf("W2=%.2f ", dbuf[1]);

    fbuf[1] = 2.5f;  (*bx.pp)[1] = 7.5f;      /* 显式解引用（原就正确 · 回归哨兵） */
    printf("W3=%.2f ", fbuf[1]);

    {   /* 局部结构体（走「非静态」分支） */
        struct box lb;
        lb.p = fbuf;  fbuf[1] = 2.5f;  lb.p[1] = 6.5f;
        printf("W4=%.2f ", fbuf[1]);
    }

    gx.p = fbuf;  fbuf[1] = 2.5f;  gx.p[1] = 5.5f;
    printf("W5=%.2f ", fbuf[1]);

    printf("R1=%.2f ", bx.p[1]);
    printf("%s\n", (bx.p == keep_p) ? "ok" : "CLOBBERED");
    return 0;
}
