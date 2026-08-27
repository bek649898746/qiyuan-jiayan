#include <stdio.h>

/* 甲言 原子比较交换 接入测试 (2026-08-28): 机制层并发 — 真 cmpxchg 原子 CAS
   原子 比较交换(&x, expected, new) → 返回旧值; [x]==expected 则 [x]=new */

int main(void) {
    int x = 5;
    /* 匹配: 旧=5, x→9 */
    int old = 原子 比较交换(&x, 5, 9);
    if (old != 5 || x != 9) { printf("FAIL cas1: old=%d x=%d\n", old, x); return 1; }
    /* 不匹配: 旧=9 (x 已是 9), x 不变 */
    old = 原子 比较交换(&x, 5, 9);
    if (old != 9 || x != 9) { printf("FAIL cas2: old=%d x=%d\n", old, x); return 1; }
    /* 变量参数 */
    int e = 9, n = 20;
    old = 原子 比较交换(&x, e, n);
    if (old != 9 || x != 20) { printf("FAIL cas3: old=%d x=%d\n", old, x); return 1; }
    /* 锁自由递增模式: 循环 CAS 直到成功 */
    int target = 100;
    int expected = target;
    int counter = 0;
    while (1) {
        int cur = 原子 比较交换(&target, expected, expected + 1);
        if (cur == expected) break; /* CAS 成功 */
        expected = cur;
        counter++;
        if (counter > 1000) { printf("FAIL cas4 loop\n"); return 1; }
    }
    if (target != 101) { printf("FAIL cas4: target=%d\n", target); return 1; }
    printf("PASS\n");
    return 0;
}
