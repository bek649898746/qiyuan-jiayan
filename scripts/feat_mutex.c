#include <stdio.h>

/* 互斥锁 + 条件变量: 锁状态 + 临界区 + 等待/唤醒 (单线程确定性模拟)
   真接入验证: 互斥性 + 所有者 + 条件等待/信号 */

typedef struct { int locked; int owner; int waiters; } Mutex;
typedef struct { int flag; int signaled; } Cond;

static void mutex_init(Mutex *m) { m->locked = 0; m->owner = 0; m->waiters = 0; }
static int mutex_lock(Mutex *m, int who) {
    if (m->locked) { m->waiters++; return -1; }   /* 忙: 等待 */
    m->locked = 1;
    m->owner = who;
    return 0;
}
static int mutex_unlock(Mutex *m, int who) {
    if (!m->locked) return -1;                    /* 未锁 */
    if (m->owner != who) return -2;               /* 非所有者 */
    m->locked = 0;
    m->owner = 0;
    if (m->waiters > 0) m->waiters--;             /* 唤醒一个等待者 */
    return 0;
}

static void cond_init(Cond *c) { c->flag = 0; c->signaled = 0; }
static void cond_wait(Cond *c, Mutex *m) {        /* 等待: 释放锁 + 阻塞 */
    if (m->locked) m->locked = 0;
    c->flag = 0;
    c->signaled = 0;
}
static void cond_signal(Cond *c) { c->signaled = 1; c->flag = 1; }

int main(void) {
    /* 1. 加锁/解锁 */
    {
        Mutex m;
        mutex_init(&m);
        if (mutex_lock(&m, 1) != 0) return 1;
        if (!m.locked || m.owner != 1) return 2;
        if (mutex_unlock(&m, 1) != 0) return 3;
        if (m.locked) return 4;
    }

    /* 2. 互斥: 持有中重入失败 + 等待计数 */
    {
        Mutex m;
        mutex_init(&m);
        mutex_lock(&m, 1);
        if (mutex_lock(&m, 2) != -1) return 5;    /* 忙 → 等待 */
        if (m.waiters != 1) return 6;
        if (mutex_unlock(&m, 1) != 0) return 7;
        if (m.waiters != 0) return 8;             /* 唤醒一个 */
    }

    /* 3. 所有者检查: 非所有者不能解锁 */
    {
        Mutex m;
        mutex_init(&m);
        mutex_lock(&m, 7);
        if (mutex_unlock(&m, 9) != -2) return 9;  /* 9 不是所有者 */
        if (mutex_unlock(&m, 7) != 0) return 10;
    }

    /* 4. 条件变量: 等待释放锁, 信号唤醒 */
    {
        Cond c;
        Mutex m;
        cond_init(&c);
        mutex_init(&m);
        mutex_lock(&m, 1);
        cond_wait(&c, &m);                        /* 等待: 释放锁 */
        if (m.locked) return 11;
        if (c.flag) return 12;
        cond_signal(&c);                          /* 信号 */
        if (!c.signaled || !c.flag) return 13;
        /* 信号后重新加锁 */
        if (mutex_lock(&m, 1) != 0) return 14;
    }

    /* 5. 锁保护计数器: 临界区操作 */
    {
        Mutex m;
        mutex_init(&m);
        int counter = 0;
        for (int i = 0; i < 5; i++) {
            mutex_lock(&m, 1);
            counter++;                            /* 临界区 */
            mutex_unlock(&m, 1);
        }
        if (counter != 5) return 15;
    }

    /* 6. 多锁独立: 两把锁互不干扰 */
    {
        Mutex a, b;
        mutex_init(&a); mutex_init(&b);
        mutex_lock(&a, 1);
        if (mutex_lock(&b, 2) != 0) return 16;    /* b 可用 (a 锁着不影响) */
        mutex_unlock(&b, 2);
        mutex_unlock(&a, 1);
    }

    printf("mutex=%d,%d cond=%d,%d\n", 1, 0, 1, 0);
    printf("PASS\n");
    return 0;
}
