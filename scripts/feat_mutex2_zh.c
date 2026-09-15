// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 互斥锁 接入测试 (2026-08-28): 机制层并发 — 内建自旋锁 (真原子 CAS): 互斥锁 m; + 锁住 m {..} */

static int counter = 0;

int work(int n) {
    互斥锁 m;
    int i;
    for (i = 0; i < n; i++) {
        锁住 m { counter++; }
    }
    return counter;
}

int early_return(int fail) {
    互斥锁 m;
    锁住 m {
        if (fail) return 42; /* 提前 return — defer 收集应解锁 */
        counter += 100;
    }
    return counter;
}

int main(void) {
    /* 基本临界区: 1000 次自旋锁保护递增 */
    int r = work(1000);
    if (r != 1000) { printf("FAIL work: %d\n", r); return 1; }
    /* 提前 return 解锁 (defer 收集) */
    counter = 0;
    int e = early_return(1);
    if (e != 42) { printf("FAIL early: %d\n", e); return 1; }
    /* 正常路径 */
    counter = 0;
    e = early_return(0);
    if (e != 100) { printf("FAIL normal: %d\n", e); return 1; }
    /* 两个独立锁嵌套 */
    互斥锁 a;
    互斥锁 b;
    锁住 a {
        锁住 b {
            counter++;
        }
    }
    if (counter != 101) { printf("FAIL nested: %d\n", counter); return 1; }
    printf("PASS\n");
    return 0;
}
