// @EXPECTED out:PASS
#include <stdio.h>

/* async/await: 事件循环 + 协程式任务 (单线程确定性模拟)
   await = 任务让出并登记期望事件; 事件到达才恢复推进 (一次事件推进一步);
   定时器 = tick 事件 (时间驱动); 完成 = 状态机终态 */

typedef struct { int state; int acc; int ticks_left; int want1; int want2; } Task;
enum { T_INIT = 0, T_WAIT_A = 1, T_WAIT_B = 2, T_WAIT_TICK = 3, T_DONE = 4 };

/* 异步任务: 等 want1 事件 → 等 want2 事件 → 等 2 个 tick → 完成
   事件0 = tick; 初始化无事件需求直接推进到等 want1 */
static int task_step(Task *t, int ev_type, int ev_val) {
    for (;;) {
        if (t->state == T_INIT) {
            t->acc = 0;
            t->ticks_left = 0;
            t->state = T_WAIT_A;
            continue;                         /* 初始化不是 await, 立即继续 */
        } else if (t->state == T_WAIT_A) {
            if (ev_type != t->want1) return 0;
            t->acc += ev_val;
            t->state = T_WAIT_B;
            continue;                         /* 当前事件只推进一步 (ev_type != want2 会返回) */
        } else if (t->state == T_WAIT_B) {
            if (ev_type != t->want2) return 0;
            t->acc += ev_val;
            t->state = T_WAIT_TICK;
            t->ticks_left = 2;
            continue;
        } else if (t->state == T_WAIT_TICK) {
            if (ev_type != 0) return 0;
            t->ticks_left--;
            if (t->ticks_left <= 0) { t->state = T_DONE; return 1; }
            return 0;
        } else {
            return 0;
        }
    }
}

/* 事件循环: 把事件投递给所有未完成任务 */
static void pump(Task *ts, int n, int ev_type, int ev_val) {
    for (int i = 0; i < n; i++) {
        if (ts[i].state != T_DONE) task_step(&ts[i], ev_type, ev_val);
    }
}

static void task_init(Task *t, int w1, int w2) {
    t->state = T_INIT;
    t->acc = 0;
    t->ticks_left = 0;
    t->want1 = w1;
    t->want2 = w2;
}

int main(void) {
    /* 1. 单任务: 事件1(5) → 事件2(3) → tick×2 → acc=8 */
    Task a;
    task_init(&a, 1, 2);
    pump(&a, 1, 1, 5);
    if (a.state != T_WAIT_B || a.acc != 5) return 1;
    pump(&a, 1, 0, 0);                        /* tick 被忽略 (还在等事件2) */
    pump(&a, 1, 2, 3);
    if (a.state != T_WAIT_TICK || a.acc != 8) return 2;
    pump(&a, 1, 0, 0);                        /* tick 1 */
    if (a.state == T_DONE) return 3;
    pump(&a, 1, 0, 0);                        /* tick 2 */
    if (a.state != T_DONE) return 4;
    if (a.acc != 8) return 5;

    /* 2. 双任务: 各自关注不同事件类型 (广播互不干扰) */
    Task pair[2];
    task_init(&pair[0], 1, 2);
    task_init(&pair[1], 3, 4);
    pump(pair, 2, 1, 10);                     /* a2 收事件1 */
    if (pair[0].state != T_WAIT_B || pair[1].state != T_WAIT_A) return 6;
    pump(pair, 2, 2, 5);                      /* a2 收事件2 → 等 tick */
    pump(pair, 2, 3, 7);                      /* b 收事件3 */
    pump(pair, 2, 4, 1);                      /* b 收事件4 → 等 tick */
    if (pair[0].acc != 15 || pair[1].acc != 8) return 7;
    pump(pair, 2, 0, 0);                      /* 给 a2 的 tick */
    pump(pair, 2, 0, 0);                      /* 完成 a2 */
    pump(pair, 2, 0, 0);                      /* 完成 b */
    if (pair[0].state != T_DONE || pair[1].state != T_DONE) return 8;

    /* 3. 事件顺序无关: 数据事件乱序到达不推进错误状态 */
    Task c;
    task_init(&c, 1, 2);
    pump(&c, 1, 2, 99);                       /* 事件2 先到 (任务在等事件1) */
    if (c.state != T_WAIT_A || c.acc != 0) return 9;
    pump(&c, 1, 1, 4);
    pump(&c, 1, 2, 6);
    if (c.acc != 10) return 10;

    /* 4. 定时器: 不同任务同节奏推进 */
    Task pair2[2];
    task_init(&pair2[0], 1, 2);
    task_init(&pair2[1], 1, 2);
    pump(pair2, 2, 1, 1);
    pump(pair2, 2, 2, 1);
    pump(pair2, 2, 0, 0);
    pump(pair2, 2, 0, 0);
    if (pair2[0].state != T_DONE) return 11;
    pump(pair2, 2, 0, 0);
    if (pair2[1].state != T_DONE) return 12;

    printf("acc=%d/%d b=%d c=%d\n", a.acc, pair[0].acc, pair[1].acc, c.acc);
    if (a.acc != 8 || pair[0].acc != 15 || pair[1].acc != 8 || c.acc != 10) return 13;
    printf("PASS\n");
    return 0;
}
