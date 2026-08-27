#include <stdio.h>

/* 协程 (stackless/async): 显式状态机 — 暂停=保存状态(state+局部变量), resume=状态分派继续
   IR v1.4 第二梯队: 恢复 continuation (状态机转换); 与生成器同构
   (yield = 保存状态+返回, resume = 按 state 恢复执行) */

typedef struct { int state; int i; int acc; } Coro;

/* 双暂停点协程: 每轮 yield 两次 — 阶段A 累加, 阶段B 翻倍
   yield 序列: 0,0, 1,2, 3,6, 6,12 然后完成 (-1) */
static int gen_steps(Coro *g) {
    for (;;) {
        if (g->state == 0) {          /* 启动: 初始化局部 */
            g->i = 0;
            g->acc = 0;
            g->state = 1;
        } else if (g->state == 1) {   /* 阶段A 暂停点 */
            if (g->i >= 4) { g->state = 3; return -1; }  /* 完成 */
            g->acc += g->i;
            g->state = 2;
            return g->acc;            /* yield 1 */
        } else if (g->state == 2) {   /* 阶段B 暂停点 */
            int v = g->acc * 2;
            g->i++;
            g->state = 1;
            return v;                 /* yield 2 */
        } else {
            return -1;                /* 完成后重复调用 */
        }
    }
}

/* 单暂停点协程: 平方数 yield 0,1,4,9,16 */
static int gen_sq(Coro *g) {
    for (;;) {
        if (g->state == 0) {
            g->i = 0;
            g->state = 1;
        } else if (g->state == 1) {
            if (g->i >= 5) { g->state = 2; return -1; }
            int v = g->i * g->i;
            g->i++;
            return v;
        } else {
            return -1;
        }
    }
}

int main(void) {
    Coro a, b;
    a.state = 0; b.state = 0; a.i = 0; b.i = 0; a.acc = 0; b.acc = 0;

    /* 双暂停点协程单跑 */
    int expect_steps[8] = {0, 0, 1, 2, 3, 6, 6, 12};
    int got[16];
    int n = 0;
    int v;
    for (;;) {
        v = gen_steps(&a);
        if (v < 0) break;
        got[n++] = v;
    }
    if (n != 8) return 1;
    for (int i = 0; i < 8; i++) if (got[i] != expect_steps[i]) return 2;

    /* 两个协程交错执行 — 状态独立 (sq 5 值 + steps 8 值, 交错后 sq 先耗尽) */
    a.state = 0; a.i = 0; a.acc = 0;
    b.state = 0; b.i = 0; b.acc = 0;
    n = 0;
    int va = 0, vb = 0;
    int expect_x[13] = {0, 0, 1, 0, 4, 1, 9, 2, 16, 3, 6, 6, 12};
    while (va >= 0 || vb >= 0) {
        va = gen_sq(&a);
        if (va >= 0) got[n++] = va;
        vb = gen_steps(&b);
        if (vb >= 0) got[n++] = vb;
    }
    if (n != 13) return 3;
    for (int i = 0; i < 13; i++) if (got[i] != expect_x[i]) return 4;

    printf("steps=%d,%d,%d,%d,%d,%d,%d,%d\n", expect_steps[0],expect_steps[1],expect_steps[2],expect_steps[3],expect_steps[4],expect_steps[5],expect_steps[6],expect_steps[7]);
    printf("x=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", got[0],got[1],got[2],got[3],got[4],got[5],got[6],got[7],got[8],got[9],got[10],got[11],got[12]);
    printf("PASS\n");
    return 0;
}
