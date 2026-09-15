// @EXPECTED out:PASS
#include <stdio.h>

/* 线程/原子/volatile: 确定性单线程并发模拟 (qcc 无 pthread/无 longjmp)
   - volatile: 跨任务共享状态, 每次访问直读内存
   - 原子: 读-改-写模拟 (单线程轮转下确定性, 语义 = 原子性验证)
   - 任务: 协程式任务 + 时间片轮转调度器 (每个任务每轮一步) */

volatile int shared_counter = 0;
volatile int flag = 0;              /* 生产者-消费者握手标志 */

/* 原子读改写: 返回旧值 */
static int atomic_fetch_add(int *p, int delta) {
    int old = *p;
    *p = old + delta;
    return old;
}

/* 任务: 每步给共享计数器 +1, 共跑 10 步 */
typedef struct { int state; int steps; int id; } Task;

static int task_inc(Task *t, volatile int *shared) {
    for (;;) {
        if (t->state == 0) {
            t->steps = 0;
            t->state = 1;
        } else if (t->state == 1) {
            if (t->steps >= 10) { t->state = 2; return 0; }
            int old = atomic_fetch_add((int *)shared, 1);
            if (old < 0) return 3;                 /* 原子返回值验证 */
            t->steps++;
            return 1;                              /* 让出 (yield) */
        } else {
            return 0;
        }
    }
}

/* 轮转调度器: 所有任务各跑一步, 直到全部完成 */
static void scheduler(Task *ts, int n) {
    int done = 0;
    while (done < n) {
        for (int i = 0; i < n; i++) {
            int r = task_inc(&ts[i], &shared_counter);
            if (r == 0 && ts[i].state == 2) done++;
        }
    }
}

/* 生产者-消费者: volatile 握手 (生产者置 flag, 消费者轮询读取) */
typedef struct { int state; int produced; } Prod;
static int producer(Prod *p) {
    for (;;) {
        if (p->state == 0) { p->produced = 0; p->state = 1; }
        else if (p->state == 1) {
            if (p->produced >= 3) { p->state = 2; return 0; }
            flag = p->produced + 1;                /* 生产: 置共享标志 */
            p->produced++;
            p->state = 1;
            return 1;
        } else return 0;
    }
}
static int consumer(Prod *c) {
    for (;;) {
        if (c->state == 0) { c->produced = 0; c->state = 1; }
        else if (c->state == 1) {
            if (flag == 0) return 1;               /* 空转等待 (轮转让出) */
            if (flag > 3) { c->state = 2; return 0; }
            /* 消费: 读后清零 */
            if (flag != c->produced + 1) return 4; /* 读到预期值 */
            c->produced = flag;
            flag = 0;
            if (c->produced >= 3) { c->state = 2; return 0; }
            return 1;
        } else return 0;
    }
}

int main(void) {
    /* 1. 3 个任务各 +10 → 共享计数器 = 30 */
    Task ts[3];
    for (int i = 0; i < 3; i++) { ts[i].state = 0; ts[i].steps = 0; ts[i].id = i; }
    scheduler(ts, 3);
    if (shared_counter != 30) return 1;

    /* 2. 调度确定性: 每次从 0 重跑结果一致 */
    shared_counter = 0;
    for (int i = 0; i < 3; i++) { ts[i].state = 0; ts[i].steps = 0; }
    scheduler(ts, 3);
    if (shared_counter != 30) return 2;

    /* 3. 生产者-消费者 volatile 握手: 3 次生产/消费 */
    flag = 0;
    Prod p, c;
    p.state = 0; p.produced = 0;
    c.state = 0; c.produced = 0;
    {
        int pdone = 0, cdone = 0;
        while (!pdone || !cdone) {
            if (!pdone) { if (producer(&p) == 0 && p.state == 2) pdone = 1; }
            if (!cdone) { if (consumer(&c) == 0 && c.state == 2) cdone = 1; }
        }
    }
    if (c.produced != 3) return 3;
    if (flag != 0) return 4;

    printf("counter=%d prod=%d flag=%d\n", shared_counter, c.produced, flag);
    if (shared_counter != 30 || c.produced != 3 || flag != 0) return 5;
    printf("PASS\n");
    return 0;
}
