#include <stdio.h>

/* 编译期执行 (Zig comptime 模拟): 宏表 + 常量折叠 — 值在编译期算好, 运行时零计算
   编译期阶乘/斐波那契表 = 宏展开计算; _Static_assert 编译期检查; enum 常量折叠 */

#define CAT_(a, b) a##b
#define CAT(a, b) CAT_(a, b)
#define FAC(n) CAT(FAC_, n)
#define FIB(n) CAT(FIB_, n)

/* 编译期阶乘表 */
#define FAC_0 1
#define FAC_1 1
#define FAC_2 2
#define FAC_3 6
#define FAC_4 24
#define FAC_5 120
#define FAC_6 720

/* 编译期斐波那契表 */
#define FIB_0 0
#define FIB_1 1
#define FIB_2 1
#define FIB_3 2
#define FIB_4 3
#define FIB_5 5
#define FIB_6 8

/* 编译期折叠的 const 数组 (运行时零计算) */
static const int facs[] = { FAC(0), FAC(1), FAC(2), FAC(3), FAC(4), FAC(5), FAC(6) };
static const int fibs[] = { FIB(0), FIB(1), FIB(2), FIB(3), FIB(4), FIB(5), FIB(6) };

/* 编译期 enum 常量折叠 */
enum { CELL = sizeof(int) * 4, HALF = CELL / 2, BIG = CELL * CELL };

/* 编译期断言 (静态检查) */
_Static_assert(FAC(5) == 120, "fac 5");
_Static_assert(FIB(6) == 8, "fib 6");
_Static_assert(sizeof(int) == 4, "int size");
_Static_assert(CELL == 16, "cell size");

int main(void) {
    /* 1. 编译期表查值 */
    if (facs[5] != 120) return 1;
    if (facs[6] != 720) return 2;
    if (fibs[6] != 8) return 3;
    if (fibs[0] != 0 || fibs[1] != 1) return 4;

    /* 2. 编译期 enum 常量 */
    if (CELL != 16 || HALF != 8 || BIG != 256) return 5;

    /* 3. 编译期表求和 (查表 = 零计算) */
    int sf = 0;
    for (int i = 0; i < 7; i++) sf += facs[i];
    if (sf != 874) return 6;               /* 1+1+2+6+24+120+720 = 874 */

    /* 4. 编译期选择 (宏按字面量参数展开 — ## 拼接需字面量 token) */
    int n = 4;
    int f;
    if (n == 4) f = FAC(4);       /* 编译期展开 = 24 */
    else if (n == 5) f = FAC(5);
    else f = -1;
    if (f != 24) return 7;

    printf("fac6=%d fib6=%d cell=%d sf=%d\n", facs[6], fibs[6], CELL, sf);
    if (facs[6] != 720 || fibs[6] != 8 || sf != 874) return 8;
    printf("PASS\n");
    return 0;
}
