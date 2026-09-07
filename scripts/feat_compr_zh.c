#include <stdio.h>

/* 甲言 列表推导 (推导 dst, x*x 于 x 遍 0 到 5) 接入测试 (2026-08-27):
 *   推导 <数组>, <元素> 于 <var> 遍 <lo> 到 <hi>;
 *   → { __ci=0; var=lo; while(var<hi){ dst[__ci]=元素; __ci++; var++; } }
 *   目标数组 + 迭代变量均须已声明
 */

int main(void) {
    /* ① 平方: sq = [0,1,4,9,16] */
    int sq[5];
    int x = 0;   /* I1 (2026-09-08): 推导 元素表达式先于 迭代名绑定, 需初值 */
    推导 sq, x * x 于 x 遍 0 到 5;
    if (sq[0] != 0 || sq[1] != 1 || sq[2] != 4 || sq[3] != 9 || sq[4] != 16) return 1;

    /* ② 非零起始: db = [2,4,6,8,10,12] (x 从 1 到 6) */
    int db[6];
    推导 db, x * 2 于 x 遍 1 到 7;
    if (db[0] != 2 || db[1] != 4 || db[5] != 12) return 2;

    /* ③ 负数表达式: neg = [0,-1,-2,-3] */
    int neg[4];
    推导 neg, 0 - x 于 x 遍 0 到 4;
    if (neg[1] != -1 || neg[3] != -3) return 3;

    /* ④ 变量界 */
    int lo = 2, hi = 5;
    int rng[3];
    推导 rng, x * 10 于 x 遍 lo 到 hi;
    if (rng[0] != 20 || rng[2] != 40) return 4;

    /* ⑤ 与夹取组合: 推导结果夹取 */
    int big[4];
    推导 big, x * 100 于 x 遍 0 到 4;
    夹取 big[2], 0, 150;   /* 200 → 150 */
    if (big[2] != 150) return 5;

    printf("PASS\n");
    return 0;
}
