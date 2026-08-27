#include <stdio.h>

/* 甲言 生成器 (生成 g 于 lo 到 hi / 下一个 g, x) 接入测试 (2026-08-27):
 *   生成 g 于 0 到 5;  →  gen_init(&g, 0, 5)
 *   下一个 g, x;      →  gen_next(&g, &x)  (返回 1 有值, 0 结束)
 *   用户定义 gen_init/gen_next (惰性 range 迭代器, C 模拟已验语义)
 */

typedef struct { int cur; int end; } Gen;

static void gen_init(Gen *g, int lo, int hi) { g->cur = lo; g->end = hi; }
static int gen_next(Gen *g, int *out) {
    if (g->cur >= g->end) return 0;
    *out = g->cur;
    g->cur++;
    return 1;
}

int main(void) {
    /* ① 基本生成: 0..4 求和 = 10 */
    Gen g;
    生成(g, 0, 5);
    int x, sum = 0, n = 0;
    while (下一个(g, x)) { sum += x; n++; }
    if (n != 5 || sum != 10) return 1;

    /* ② 非零起始: 3..6 = 3+4+5 = 12 */
    Gen g2;
    生成(g2, 3, 6);
    sum = 0;
    while (下一个(g2, x)) sum += x;
    if (sum != 12) return 2;

    /* ③ 变量界 */
    int lo = 1, hi = 4;
    Gen g3;
    生成(g3, lo, hi);
    sum = 0;
    while (下一个(g3, x)) sum += x;
    if (sum != 1 + 2 + 3) return 3;

    /* ④ 与已有特性组合: 生成值夹取 */
    Gen g4;
    生成(g4, 0, 10);
    int count = 0;
    while (下一个(g4, x)) {
        夹取 x, 2, 8;   /* 修正后全在 [2,8] */
        if (x >= 2 && x <= 8) count++;
    }
    if (count != 10) return 4;   /* 0..9 共 10 个, 夹取后都在 [2,8] */

    printf("PASS\n");
    return 0;
}
