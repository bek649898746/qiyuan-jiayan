// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 编译期执行 (编译期(expr)) 接入测试 (2026-08-27):
 *   编译期(expr) → const_expr_eval 编译期求值 → 常量节点 (运行时零计算)
 */

/* 编译期宏表 (Zig comptime 模拟) */
#define FAC(n) FAC_##n
#define FAC_0 1
#define FAC_1 1
#define FAC_2 2
#define FAC_3 6
#define FAC_4 24
#define FAC_5 120
#define FAC_6 720

enum { CELL = sizeof(int) * 4, HALF = CELL / 2, BIG = CELL * CELL };

int main(void) {
    /* ① 算术表达式编译期求值 */
    int a = 编译期(5 * 6);
    if (a != 30) return 1;

    /* ② 宏查表编译期求值 */
    int b = 编译期(FAC(4));
    if (b != 24) return 2;

    /* ③ enum 常量编译期求值 */
    int c = 编译期(CELL + HALF);
    if (c != 24) return 3;

    /* ④ 嵌套表达式 */
    int d = 编译期((2 + 3) * (7 - 1));
    if (d != 30) return 4;

    /* ⑤ 与已有特性组合 */
    范围 d 于 0 到 100;
    printf("a=%d b=%d c=%d d=%d\n", a, b, c, d);
    printf("PASS\n");
    return 0;
}
