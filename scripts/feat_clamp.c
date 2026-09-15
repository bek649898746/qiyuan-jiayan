// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 夹取 (夹取 x, lo, hi) 接入测试 (2026-08-27):
 *   夹取 x, 1, 10; → if (x < 1) x = 1; if (x > 10) x = 10;
 *   越界修正不回退 (与范围检查"越界终止"互补)
 */

int main(void) {
    /* ① 低于下界 → 修正到下界 */
    int a = -5;
    夹取 a, 1, 10;
    if (a != 1) return 1;

    /* ② 高于上界 → 修正到上界 */
    int b = 50;
    夹取 b, 1, 10;
    if (b != 10) return 2;

    /* ③ 范围内 → 不变 */
    int c = 7;
    夹取 c, 1, 10;
    if (c != 7) return 3;

    /* ④ 变量界 */
    int x = 100;
    int l = 20, h = 30;
    夹取 x, l, h;
    if (x != 30) return 4;

    /* ⑤ 边界值 (闭区间) */
    int lo = 1, hi = 10;
    夹取 lo, 1, 10;
    夹取 hi, 1, 10;
    if (lo != 1 || hi != 10) return 5;

    /* ⑥ 连续夹取 */
    int y = 1000;
    夹取 y, 0, 100;
    夹取 y, 10, 50;
    if (y != 50) return 6;

    /* ⑦ 与范围检查组合: 夹取修正后范围通过 */
    int v = 500;
    夹取 v, 1, 10;
    范围 v 于 1 到 10;   /* 修正后必然通过 */
    if (v != 10) return 7;

    printf("PASS\n");
    return 0;
}
