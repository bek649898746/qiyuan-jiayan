#include <stdio.h>

/* 甲言 异步 (异步 <op>(args) → async_<op>(args)) 接入测试 (2026-08-27):
 *   异步 泵(ts, 3, 1, 10);    →  async_pump(ts, 3, 1, 10)
 *   异步 步进(t, 1, 10);      →  async_step(t, 1, 10)
 *   用户定义 async_* (事件循环+协程式 await+定时器 tick, C 模拟已验语义)
 */

typedef struct { int state; int acc; int ticks_left; int want1; int want2; } Task;
enum { T_INIT = 0, T_WAIT_A = 1, T_WAIT_B = 2, T_WAIT_TICK = 3, T_DONE = 4 };

/* 协程式异步任务: 等 want1 事件 → 等 want2 事件 → 等 2 个 tick → 完成 */
static int async_step(Task *t, int ev_type, int ev_val) {
    for (;;) {
        if (t->state == T_INIT) {
            t->acc = 0; t->ticks_left = 0; t->state = T_WAIT_A;
            continue;
        } else if (t->state == T_WAIT_A) {
            if (ev_type != t->want1) return 0;
            t->acc += ev_val; t->state = T_WAIT_B;
            continue;
        } else if (t->state == T_WAIT_B) {
            if (ev_type != t->want2) return 0;
            t->acc += ev_val; t->state = T_WAIT_TICK; t->ticks_left = 2;
            continue;
        } else if (t->state == T_WAIT_TICK) {
            if (ev_type != 0) return 0;
            t->ticks_left--;
            if (t->ticks_left <= 0) { t->state = T_DONE; return 1; }
            return 0;
        } else return 0;
    }
}
/* 事件循环: 事件投递给所有未完成任务 */
static void async_pump(Task *ts, int n, int ev_type, int ev_val) {
    for (int i = 0; i < n; i++) {
        if (ts[i].state != T_DONE) async_step(&ts[i], ev_type, ev_val);
    }
}

int main(void) {
    /* ① 两个异步任务: 等事件 1→2 + 2 个 tick 完成 */
    Task ts[2];
    for (int i = 0; i < 2; i++) { ts[i].state = T_INIT; ts[i].want1 = 1; ts[i].want2 = 2; }

    异步 泵(ts, 2, 1, 10);    /* async_pump: 事件1 → 任务推进到等 want2 */
    异步 泵(ts, 2, 2, 20);    /* 事件2 → 推进到等 tick */
    异步 泵(ts, 2, 0, 0);     /* tick 1 */
    异步 泵(ts, 2, 0, 0);     /* tick 2 → 完成 */

    if (ts[0].state != T_DONE || ts[1].state != T_DONE) return 1;
    if (ts[0].acc != 30 || ts[1].acc != 30) return 2;

    /* ② 与已有特性组合 */
    夹取 ts[0].acc, 0, 25;   /* 30 → 25 */
    if (ts[0].acc != 25) return 3;

    printf("PASS\n");
    return 0;
}
