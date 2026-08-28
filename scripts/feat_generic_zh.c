#include <stdio.h>

/* 甲言 泛型 接入测试 (2026-08-28): 机制层 #96 — 泛型 名字(参数) = 体; 注册 fn_macro (文本级模板),
   调用按实参类型展开 (天然多类型): 泛型 最大值(x, y) = ((x) > (y) ? (x) : (y)); */

泛型 最大值(x, y) = ((x) > (y) ? (x) : (y));
泛型 求和2(a, b) = ((a) + (b));

int main(void) {
    /* int 实例化 */
    int a = 最大值(3, 7);
    if (a != 7) { printf("FAIL g1: %d\n", a); return 1; }
    /* double 实例化 */
    double c = 最大值(3.5, 2.5);
    if (c != 3.5) { printf("FAIL g2: %.1f\n", c); return 1; }
    /* 变量实参 */
    int x = 10, y = 4;
    int d = 最大值(x, y);
    if (d != 10) { printf("FAIL g3: %d\n", d); return 1; }
    /* 运算组合 */
    int e = 最大值(1, 2) + 最大值(5, 3);
    if (e != 7) { printf("FAIL g4: %d\n", e); return 1; }
    /* 第二个泛型 */
    int f = 求和2(20, 22);
    if (f != 42) { printf("FAIL g5: %d\n", f); return 1; }
    /* 嵌套调用 */
    int g = 最大值(最大值(1, 5), 最大值(3, 2));
    if (g != 5) { printf("FAIL g6: %d\n", g); return 1; }
    printf("PASS\n");
    return 0;
}
