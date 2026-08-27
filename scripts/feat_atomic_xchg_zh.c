#include <stdio.h>

/* 甲言 原子加(真xadd)/原子交换(xchg) 接入测试 (2026-08-28): 机制层并发 — 真原子指令 (非用户函数) */

int main(void) {
    int x = 5;
    /* 原子 加: 真 lock xadd — 返回旧值, x += delta */
    int old = 原子 加(&x, 3);
    if (old != 5 || x != 8) { printf("FAIL add1: old=%d x=%d\n", old, x); return 1; }
    old = 原子 加(&x, -2);
    if (old != 8 || x != 6) { printf("FAIL add2: old=%d x=%d\n", old, x); return 1; }
    /* 原子 交换: xchg — 返回旧值, x = v */
    old = 原子 交换(&x, 100);
    if (old != 6 || x != 100) { printf("FAIL xchg1: old=%d x=%d\n", old, x); return 1; }
    /* 变量参数 */
    int d = 10;
    old = 原子 加(&x, d);
    if (old != 100 || x != 110) { printf("FAIL add3: old=%d x=%d\n", old, x); return 1; }
    /* 组合: CAS + 加 + 交换 */
    old = 原子 比较交换(&x, 110, 0);
    if (old != 110 || x != 0) { printf("FAIL combo cas: old=%d x=%d\n", old, x); return 1; }
    old = 原子 加(&x, 7);
    if (old != 0 || x != 7) { printf("FAIL combo add: old=%d x=%d\n", old, x); return 1; }
    old = 原子 交换(&x, 42);
    if (old != 7 || x != 42) { printf("FAIL combo xchg: old=%d x=%d\n", old, x); return 1; }
    printf("PASS\n");
    return 0;
}
