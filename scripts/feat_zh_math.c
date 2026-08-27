#include <stdio.h>
#include <math.h>

/* 甲言 完全中文化: 数学库函数中文映射测试 (2026-08-27):
 *   取平方根→sqrt, 取幂→pow, 上限→ceil, 下限→floor,
 *   取正弦→sin, 取余弦→cos, 取正切→tan, 取余数→fmod, 取随机→rand, 设种子→srand
 */

int main(void) {
    /* ① 平方根 */
    double r = 取平方根(16.0);   /* sqrt(16) = 4 */
    if (r != 4.0) return 1;

    /* ② 幂 */
    double p = 取幂(2.0, 3.0);   /* pow(2,3) = 8 */
    if (p != 8.0) return 2;

    /* ③ 上下限 */
    double c = 上限(3.2);        /* ceil(3.2) = 4 */
    double f = 下限(3.8);        /* floor(3.8) = 3 */
    if (c != 4.0 || f != 3.0) return 3;

    /* ④ 三角 */
    double s = 取正弦(0.0);      /* sin(0) = 0 */
    double cs = 取余弦(0.0);     /* cos(0) = 1 */
    if (s != 0.0 || cs != 1.0) return 4;

    /* ⑤ 余数 */
    double m = 取余数(7.5, 2.0); /* fmod(7.5,2) = 1.5 */
    if (m != 1.5) return 5;

    /* ⑥ 随机数 */
    设种子(42);                  /* srand(42) */
    int x = 取随机() % 100;      /* rand() % 100 */
    if (x < 0 || x >= 100) return 6;

    /* ⑦ 与已有特性组合 */
    夹取 x, 0, 50;
    范围 x 于 0 到 50;

    printf("PASS\n");
    return 0;
}
