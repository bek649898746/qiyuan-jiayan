#include <stdio.h>

/* 变参模板 (C++20): C 用 X-macro + 变参宏模拟
   模板 = 宏生成多类型实例 (编译期展开), 变参 = 不定参数宏
   (注: qcc 宏重定义 #undef+#define 不同步 — 挂清理包; 用单 X-macro 一次生成) */

/* X-macro: 一个宏定义同时生成 sum_<T> + sz_<T> + swap_<T> (模板实例化模拟) */
#define TYPES_X \
    X(int, 4) \
    X(long, 8) \
    X(double, 8)

#define X(T, SZ) \
    static T sum_##T(T a, T b) { return a + b; } \
    static int sz_##T(void) { return SZ; } \
    static void swap_##T(T *a, T *b) { T t = *a; *a = *b; *b = t; }
TYPES_X
#undef X

/* 变参宏: 不定参数 (最多 4 个) 求和 */
#define SUM4(a, b, c, d) ((a) + (b) + (c) + (d))

int main(void) {
    /* 1. X-macro 生成的 sum 函数族 */
    int si = sum_int(3, 4);
    long sl = sum_long(5, 6);
    double sd = sum_double(1.5, 2.5);
    if (si != 7) return 1;
    if (sl != 11) return 2;
    if (sd != 4.0) return 3;

    /* 2. X-macro 生成的 sz 函数族 */
    if (sz_int() != 4) return 4;
    if (sz_long() != 8) return 5;
    if (sz_double() != 8) return 6;

    /* 3. 变参宏 */
    int s4 = SUM4(1, 2, 3, 4);
    if (s4 != 10) return 7;
    int s2 = SUM4(10, 20, 0, 0);
    if (s2 != 30) return 8;

    /* 4. X-macro 生成的 swap 实例化 */
    int x = 1, y = 2;
    swap_int(&x, &y);
    if (x != 2 || y != 1) return 9;
    double dx = 1.5, dy = 2.5;
    swap_double(&dx, &dy);
    if (dx != 2.5 || dy != 1.5) return 10;

    /* 5. 组合: 模板实例 + 变参 */
    int r = sum_int(si, s4);
    if (r != 17) return 11;

    printf("si=%d sl=%d sd=%d s4=%d swap=(%d,%d)\n", si, (int)sl, (int)sd, s4, x, y);
    printf("PASS\n");
    return 0;
}
