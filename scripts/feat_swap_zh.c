// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 交换 (交换 a, b) 接入测试 (2026-08-27):
 *   交换 a, b;  →  { int __sw=a; a=b; b=__sw; }
 *   临时变量中转, 排序/算法常用
 */

int main(void) {
    /* ① 基本交换 */
    int x = 1, y = 2;
    交换 x, y;
    if (x != 2 || y != 1) return 1;

    /* ② 数组元素交换 */
    int arr[3] = { 10, 20, 30 };
    交换 arr[0], arr[2];
    if (arr[0] != 30 || arr[2] != 10) return 2;

    /* ③ 与排序模式组合 (冒泡一趟) */
    int a = 5, b = 3;
    if (a > b) 交换 a, b;
    if (a != 3 || b != 5) return 3;

    /* ④ 与已有特性组合 */
    夹取 x, 0, 10;
    范围 y 于 0 到 10;
    if (x != 2 || y != 1) return 4;

    printf("PASS\n");
    return 0;
}
