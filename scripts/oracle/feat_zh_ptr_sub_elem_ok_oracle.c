/* oracle for scripts/feat_zh_ptr_sub_elem_ok.c — 逐值对照 (供 gcc 跑)
   R273 (2026-09-26 启元 · 第 364 站 round 12): 指针相减 按元素尺寸归一.
   甲言侧 `pa[1] - pa[0]` 是**下标结果相减**（两个指针）⟹ 结果 = 字节差 ÷ 元素宽;
   gcc 侧同一写法天然正确 ⟹ 两份输出应**逐字相同**: `P i=3 c=7 s=2 t=2 v=4`。
   四元素宽各自归一: 整 4 / 字 1 / 构点 8 / 构三 12; 值数组相减 4 作守门（不该缩放）。 */
#include <stdio.h>

struct 点 { int x; int y; };
struct 三 { int a; int b; int c; };

int main(void) {
    int ia[4]; ia[0] = 10; ia[3] = 40;
    int *pa[2]; pa[0] = &ia[0]; pa[1] = &ia[3];

    char ca[8]; ca[0] = 65; ca[7] = 72;
    char *cp[2]; cp[0] = &ca[0]; cp[1] = &ca[7];

    struct 点 sa[3]; sa[0].x = 1; sa[2].x = 3;
    struct 点 *sp[2]; sp[0] = &sa[0]; sp[1] = &sa[2];

    struct 三 sa3[3]; sa3[0].a = 1; sa3[2].a = 3;
    struct 三 *tp3[2]; tp3[0] = &sa3[0]; tp3[1] = &sa3[2];

    int vb[2]; vb[0] = 5; vb[1] = 9;

    printf("P i=%d c=%d s=%d t=%d v=%d\n",
           (int)(pa[1] - pa[0]), (int)(cp[1] - cp[0]),
           (int)(sp[1] - sp[0]), (int)(tp3[1] - tp3[0]), vb[1] - vb[0]);
    return 0;
}
