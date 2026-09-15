// @EXPECTED out:PASS
#include <stdio.h>

/* 生成器 (generator): 协程链的惰性迭代器 — 不预生成, next() 时才计算产出
   IR v1.4 第二梯队: 协程链; 与协程同构 (yield=产出, resume=续算)
   惰性语义: 构造零开销, 逐个产出, 完成后恒 -1 */

typedef struct { int state; int cur; int end; int step; } Gen;

static void gen_init(Gen *g, int from, int to, int step) {
    g->state = 0;
    g->cur = from;
    g->end = to;
    g->step = step;
}

/* range 生成器: 惰性产出 [from,to) 步进 step */
static int gen_next(Gen *g) {
    for (;;) {
        if (g->state == 0) {
            g->state = 1;
        } else if (g->state == 1) {
            if ((g->step > 0 && g->cur >= g->end) || (g->step < 0 && g->cur <= g->end)) {
                g->state = 2;
                return -1;
            }
            int v = g->cur;
            g->cur += g->step;
            return v;                 /* yield */
        } else {
            return -1;                /* 完成 */
        }
    }
}

/* 惰性求值验证: 不预先分配数组 — 消费端直接累计 */
static int gen_sum(Gen *g) {
    int s = 0;
    int v;
    while ((v = gen_next(g)) >= 0) s += v;
    return s;
}

int main(void) {
    Gen g;

    /* 1. 正向 range: 0..4 */
    gen_init(&g, 0, 5, 1);
    int seq[8];
    int n = 0;
    int v;
    while ((v = gen_next(&g)) >= 0) seq[n++] = v;
    if (n != 5) return 1;
    for (int i = 0; i < 5; i++) if (seq[i] != i) return 2;

    /* 2. 负步长 range: 10,8,6,4,2 */
    gen_init(&g, 10, 0, -2);
    n = 0;
    while ((v = gen_next(&g)) >= 0) seq[n++] = v;
    if (n != 5) return 3;
    if (seq[0] != 10 || seq[1] != 8 || seq[4] != 2) return 4;

    /* 3. 惰性累计 (sum 1+2+...+100 = 5050) */
    gen_init(&g, 1, 101, 1);
    if (gen_sum(&g) != 5050) return 5;

    /* 4. 两个生成器交错 — 状态独立 (range 0..2 和 10..12) */
    Gen g1, g2;
    gen_init(&g1, 0, 3, 1);
    gen_init(&g2, 10, 13, 1);
    int x[12];
    n = 0;
    int v1 = 0, v2 = 0;
    while (v1 >= 0 || v2 >= 0) {
        v1 = gen_next(&g1);
        if (v1 >= 0) x[n++] = v1;
        v2 = gen_next(&g2);
        if (v2 >= 0) x[n++] = v2;
    }
    if (n != 6) return 6;
    int expect[6] = {0, 10, 1, 11, 2, 12};
    for (int i = 0; i < 6; i++) if (x[i] != expect[i]) return 7;

    /* 5. 生成器复用: 完成后重置再产出 */
    gen_init(&g, 7, 9, 1);
    n = 0;
    while ((v = gen_next(&g)) >= 0) seq[n++] = v;
    gen_init(&g, 20, 22, 1);
    n = 0;
    while ((v = gen_next(&g)) >= 0) seq[n++] = v;
    if (seq[0] != 20 || seq[1] != 21) return 8;

    printf("r1=%d,%d,%d,%d,%d r2=%d,%d,%d,%d,%d sum=%d\n", 0,1,2,3,4, 10,8,6,4,2, 5050);
    printf("PASS\n");
    return 0;
}
