// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 volatile 中文化 (易失) + 屏障 接入测试 (2026-08-27):
 *   易失 int x;  →  volatile int x
 *   屏障();      →  编译器屏障 (单线程确定性下无操作)
 */

int main(void) {
    /* ① 易失 声明 */
    易失 int shared = 0;       /* volatile int shared */
    shared = 5;
    if (shared != 5) return 1;

    /* ② 屏障 */
    屏障();
    shared = 10;
    屏障();
    if (shared != 10) return 2;

    /* ③ 与已有特性组合 (普通 int) */
    int x = 15;
    夹取 x, 0, 12;             /* 15 → 12 */
    范围 x 于 0 到 12;
    if (x != 12) return 3;

    printf("PASS\n");
    return 0;
}
