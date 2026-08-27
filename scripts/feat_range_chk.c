#include <stdio.h>

/* 甲言 范围检查 接入测试 (2026-08-27):
 *   范围 x 于 1 到 10; → if (x < 1) { printf("范围越界\n"); exit(1); }
 *                            if (x > 10) { printf("范围越界\n"); exit(1); }
 *   Ada range 约束的运行时检查
 */

int main(void) {
    /* ① 范围内: 通过 */
    int a = 5;
    范围 a 于 1 到 10;
    if (a != 5) return 1;

    /* ② 边界值: 1 和 10 都合法 (闭区间) */
    int lo = 1, hi = 10;
    范围 lo 于 1 到 10;
    范围 hi 于 1 到 10;
    if (lo != 1 || hi != 10) return 2;

    /* ③ 变量界 */
    int x = 7;
    int l = 3, h = 9;
    范围 x 于 l 到 h;
    if (x != 7) return 3;

    /* ④ 表达式 */
    int y = 15;
    范围 y * 2 于 0 到 100;   /* 30 ∈ [0,100] 通过 */
    if (y != 15) return 4;

    /* ⑤ 负数范围 */
    int n = -3;
    范围 n 于 -5 到 5;
    if (n != -3) return 5;

    /* ⑥ 与已有特性组合 (可空+范围) */
    int arr[3] = { 1, 2, 3 };
    可空 int *p = (arr[1] == 2) ? &arr[1] : 0;
    解包 p;
    范围 *p 于 0 到 9;
    if (*p != 2) return 6;

    printf("PASS\n");
    return 0;
}
