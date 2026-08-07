// @EXPECTED exit:0
// @EXPECTED out:deep-ok
#include <stdio.h>
/* deep regression: static struct + big struct by-value param + array elem field (2026-08-07) */
struct Sbig { char name[64]; double d; int v[3]; };
static int g_cnt = 5;              /* cg_f t==1 int static in double expr */
static double g_d = 2.5;
struct Sbig g_big;                 /* static struct */
static struct Sbig *g_ptr;         /* static ptr arrow rw */

int take_big(struct Sbig s) { return (int)s.d + s.v[0]; }  /* big struct by value */
void tset_big(struct Sbig s) { s.v[0] = 999; }            /* 按值修改: 不影响原数组 */

int main(void) {
    double x = g_cnt * 1.0;
    if (x != 5.0) return 1;

    if (g_d != 2.5) return 2;

    g_big.d = 3.5; g_big.v[0] = 7;
    if (g_big.d != 3.5) return 3;
    if (g_big.v[0] != 7) return 4;

    g_ptr = &g_big;
    g_ptr->d = 4.25;
    if (g_ptr->d != 4.25) return 5;
    g_ptr->v[0] = 42;
    if (g_ptr->v[0] != 42) return 6;

    {
        struct Sbig arr[3];
        arr[1].d = 8.5; arr[1].v[0] = 9;
        if (take_big(arr[1]) != 17) return 7;
        arr[1].d = 0.0; arr[1].v[0] = -14;
        if (take_big(arr[1]) != -14) return 10;   /* 负 int 数组元素: 原 movzbl 读 8 位 → 242 (fix 2026-08-07) */
    }

    g_big.d = 4.25;
    if (take_big(g_big) != 46) return 8;

    /* 静态结构体数组元素按值传参: 被调方修改不得影响原数组 (fix 2026-08-07 别名 bug) */
    {
        static struct Sbig sg[2];
        sg[1].v[0] = 11;
        tset_big(sg[1]);
        if (sg[1].v[0] != 11) return 11;
        sg[1].d = 2.0; sg[1].v[0] = 6;
        if (take_big(sg[1]) != 8) return 12;
    }

    if (g_big.d * 2.0 != 8.5) return 9;

    printf("deep-ok\n");
    return 0;
}
