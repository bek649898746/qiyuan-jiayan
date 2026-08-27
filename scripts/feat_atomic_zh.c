#include <stdio.h>

/* 甲言 原子操作 (原子 加/读/写 → atomic_fetch_add/load/store) 接入测试 (2026-08-27):
 *   原子 加(&x, 1);  →  atomic_fetch_add(&x, 1)  (返回旧值)
 *   原子 读(&x);     →  atomic_load(&x)
 *   原子 写(&x, v);  →  atomic_store(&x, v)
 *   用户定义 atomic_* (读改写原子性, C 模拟已验语义)
 */

static volatile int shared = 0;

static int atomic_fetch_add(int *p, int delta) {
    int old = *p;
    *p = old + delta;
    return old;
}
static int atomic_load(int *p) { return *p; }
static void atomic_store(int *p, int v) { *p = v; }

int main(void) {
    /* ① 原子加 (返回旧值) */
    int old = 原子 加(&shared, 5);
    if (old != 0 || shared != 5) return 1;
    old = 原子 加(&shared, 3);
    if (old != 5 || shared != 8) return 2;

    /* ② 原子读 */
    int v = 原子 读(&shared);
    if (v != 8) return 3;

    /* ③ 原子写 */
    原子 写(&shared, 100);
    if (shared != 100) return 4;

    /* ④ 与线程/已有特性组合 */
    夹取 shared, 0, 50;   /* 100 → 50 */
    范围 shared 于 0 到 50;
    if (shared != 50) return 5;

    printf("PASS\n");
    return 0;
}
