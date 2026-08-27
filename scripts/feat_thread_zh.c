#include <stdio.h>

/* 甲言 线程 (线程 t 跑 work, arg / 等待 t) 接入测试 (2026-08-27):
 *   线程 t 跑 fn, arg; → thread_create(&t, fn, arg);
 *   等待 t;            → thread_join(&t);
 *   库级语法糖: 用户定义 thread_create/thread_join (确定性单线程模拟)
 */

typedef struct { int (*fn)(void*, void*); void *arg; int state; int steps; } Thread;

static int thread_create(Thread *t, int (*fn)(void*, void*), void *arg) {
    t->fn = fn; t->arg = arg; t->state = 0; t->steps = 0;
    return 0;
}
static int thread_join(Thread *t) {
    while (t->state != 2) { if (t->fn(t->arg, t) == 0) t->state = 2; }
    return 0;
}

/* 任务: 累加 0..4 = 10 (每次一步, 确定性单线程) */
static int work(void *arg, void *tctx) {
    Thread *t = (Thread*)tctx;
    int *sum = (int*)arg;
    if (t->steps < 5) { *sum += t->steps; t->steps++; return 1; }
    return 0;
}

int main(void) {
    /* ① 单线程 */
    int sum = 0;
    Thread t;
    线程 t 跑 work, &sum;
    等待 t;
    if (sum != 10) return 1;

    /* ② 双线程独立 */
    int s1 = 0, s2 = 0;
    Thread t1, t2;
    线程 t1 跑 work, &s1;
    线程 t2 跑 work, &s2;
    等待 t1;
    等待 t2;
    if (s1 != 10 || s2 != 10) return 2;

    /* ③ 线程 + 通道组合 (确定性并发) */
    int acc = 0;
    Thread ta;
    线程 ta 跑 work, &acc;
    等待 ta;
    夹取 acc, 0, 5;   /* 10 → 5 */
    if (acc != 5) return 3;

    printf("PASS\n");
    return 0;
}
