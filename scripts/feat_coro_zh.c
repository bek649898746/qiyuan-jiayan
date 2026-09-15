// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 协程 (协程(c, fn, ctx) / 步进(c)) 接入测试 (2026-08-27):
 *   协程(c, fn, ctx);  →  coro_init(&c, fn, ctx)
 *   步进(c);           →  coro_step(&c)  (返回 1 运行中, 0 完成)
 *   用户定义 coro_init/coro_step (确定性 stackless 状态机协程, C 模拟已验语义)
 */

typedef struct { int (*fn)(void*, int); void *ctx; int state; } Coro;

static void coro_init(Coro *c, int (*fn)(void*, int), void *ctx) {
    c->fn = fn; c->ctx = ctx; c->state = 0;
}
static int coro_step(Coro *c) {
    int r = c->fn(c->ctx, c->state);
    if (r < 0) { c->state = -1; return 0; }  /* 完成 */
    c->state = r;
    return 1;  /* 运行中 */
}

/* 状态机协程: 双暂停点 (C 模拟: 每步 state 推进, -1 结束), 累加 0..3 = 6 */
typedef struct { int sum; int n; } Ctx;
static int counter_coro(void *vctx, int st) {
    Ctx *ctx = (Ctx*)vctx;
    if (st == 0) { ctx->n = 0; return 1; }        /* 暂停点 1 */
    if (st == 1) { if (ctx->n < 4) { ctx->sum += ctx->n; ctx->n++; return 1; } return -1; }
    return -1;
}

int main(void) {
    /* ① 协程步进直到完成 */
    Ctx ctx = { 0, 0 };
    Coro c;
    协程(c, counter_coro, &ctx);
    int running = 1, steps = 0;
    while (running) { running = 步进(c); steps++; }
    if (ctx.sum != 0 + 1 + 2 + 3) return 1;
    if (steps < 4) return 2;   /* 至少 4 步 */

    /* ② 与已有特性组合 */
    夹取 ctx.sum, 0, 5;   /* 6 → 5 */
    if (ctx.sum != 5) return 3;

    printf("PASS\n");
    return 0;
}
