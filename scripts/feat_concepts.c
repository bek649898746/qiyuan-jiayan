#include <stdio.h>

/* concepts 类型约束 (C++20): C 用 _Generic 类型判定模拟
   概念 = 编译期类型属性检查 — 类型满足概念才选路 (_Generic 编译期选分支) */

#define IS_INT(T) _Generic((T)0, int: 1, long: 1, short: 1, char: 1, default: 0)
#define IS_PTR(T) _Generic((T)0, int *: 1, char *: 1, void *: 1, default: 0)
#define IS_DBL(T) _Generic((T)0, double: 1, float: 1, default: 0)

/* 受约束操作: 概念满足才执行 (运行时断言 + 类型查询) */
static int require_int(int ok, int v) {
    if (!ok) return -1;
    return v;
}

int main(void) {
    /* 1. 概念判定 (局部变量 — 编译期 _Generic 在 ginit/_Static_assert/if条件 挂清理包) */
    int CON_INT = IS_INT(int);
    int CON_PTR = IS_PTR(int *);
    int CON_DBL = IS_DBL(double);
    int n_int_dbl = IS_INT(double);
    int n_ptr_int = IS_PTR(int);
    int n_dbl_int = IS_DBL(int);
    int y_int_char = IS_INT(char);
    int y_ptr_char = IS_PTR(char *);
    if (CON_INT != 1) return 1;
    if (CON_PTR != 1) return 2;
    if (CON_DBL != 1) return 3;
    if (n_int_dbl != 0) return 4;
    if (n_ptr_int != 0) return 5;
    if (n_dbl_int != 0) return 6;
    if (y_int_char != 1) return 7;
    if (y_ptr_char != 1) return 8;

    /* 2. 概念约束: 满足概念的实参被接受, 不满足被拒绝 */
    int r = require_int(IS_INT(int), 42);
    if (r != 42) return 9;
    r = require_int(IS_INT(double), 7);
    if (r != -1) return 10;          /* double 不满足 int 概念 → 拒绝 */

    /* 3. 概念 + 反射: 约束通过时大小查询 */
    if (CON_INT && sizeof(int) != 4) return 11;
    if (CON_PTR && sizeof(int *) != 8) return 12;

    /* 4. 多概念组合 */
    int ok = IS_INT(int) && IS_DBL(double) && IS_PTR(char *);
    if (!ok) return 13;
    ok = IS_INT(int) && IS_DBL(int);
    if (ok) return 14;

    printf("int=%d ptr=%d dbl=%d r=%d\n", CON_INT, CON_PTR, CON_DBL, r);
    printf("PASS\n");
    return 0;
}
