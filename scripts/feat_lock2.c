// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言互斥锁 (锁住) defer 升级: 锁住 m { body } → mutex_lock(&m); defer mutex_unlock(&m); body
   函数退出 epi_label 处自动解锁 — 提前 return/break 也解锁 (Go 风格) */

static int m = 0;
static void mutex_lock(int *p) { (*p)++; }
static void mutex_unlock(int *p) { (*p)--; }

/* 提前 return: defer 解锁 */
static int try_early(int b) {
    锁住 m {
        if (b == 0) return 1;    /* 提前返回 — 但仍解锁 */
    }
    return 0;
}

/* 正常路径: 函数尾解锁 */
static int try_normal(int v) {
    锁住 m {
        if (v > 0) return v;     /* 条件 return — 解锁 */
    }
    return 0;
}

int main(void) {
    /* 1. 提前 return 也解锁 */
    m = 0;
    int r = try_early(0);
    if (r != 1) return 1;
    if (m != 0) return 2;        /* defer 已解锁 */

    /* 2. 正常路径解锁 */
    m = 0;
    r = try_early(1);
    if (r != 0) return 3;
    if (m != 0) return 4;

    /* 3. 条件 return 解锁 */
    m = 0;
    r = try_normal(5);
    if (r != 5) return 5;
    if (m != 0) return 6;        /* 条件 return 也解锁 */

    /* 4. 多函数锁独立 */
    m = 0;
    r = try_early(0);
    if (m != 0) return 7;
    r = try_normal(0);
    if (m != 0) return 8;

    printf("e=%d n=%d v=%d\n", 1, 0, 5);
    printf("PASS\n");
    return 0;
}
