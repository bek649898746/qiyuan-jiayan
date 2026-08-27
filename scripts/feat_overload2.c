#include <stdio.h>

/* 运算符重载 (C++): a+b 分派到类型对应实现 — _Generic 类型分派 (返回兼容 int)
   复数用单独函数 (struct 不能进 _Generic 兼容分支) */

typedef struct { int re; int im; } Complex;

static int add_int(int a, int b) { return a + b; }
static int add_dbl(double a, double b) { return (int)(a + b); }
static int sub_int(int a, int b) { return a - b; }

/* operator+ 模拟: int/double 分派 (结果统一 int 保证 _Generic 类型兼容) */
#define OP_ADD(a, b) _Generic((a), int: add_int((a), (b)), double: add_dbl((a), (b)))
#define OP_SUB(a, b) _Generic((a), int: sub_int((a), (b)), double: add_dbl((a), (b)))

/* 自定义类型重载: 独立函数 (分派表模拟) */
static Complex c_add(Complex a, Complex b) {
    Complex r;
    r.re = a.re + b.re;
    r.im = a.im + b.im;
    return r;
}
static Complex c_sub(Complex a, Complex b) {
    Complex r;
    r.re = a.re - b.re;
    r.im = a.im - b.im;
    return r;
}

/* 运算符分派表: 类型 → 函数 (动态重载) */
typedef struct { int (*add)(int, int); int (*mul)(int, int); } OpTbl;
static int t_add(int a, int b) { return a + b; }
static int t_mul(int a, int b) { return a * b; }
static const OpTbl int_tbl = { t_add, t_mul };

int main(void) {
    /* 1. int 加法重载 */
    int si = OP_ADD(3, 4);
    if (si != 7) return 1;

    /* 2. double 加法重载 (结果 int) */
    int sd = OP_ADD(1.5, 2.5);
    if (sd != 4) return 2;

    /* 3. int 减法重载 */
    int di = OP_SUB(10, 3);
    if (di != 7) return 3;

    /* 4. 自定义类型: Complex 加法 */
    {
        Complex a, b, r;
        a.re = 1; a.im = 2;
        b.re = 3; b.im = 4;
        r = c_add(a, b);
        if (r.re != 4 || r.im != 6) return 4;
    }

    /* 5. Complex 减法 */
    {
        Complex a, b, r;
        a.re = 5; a.im = 7;
        b.re = 2; b.im = 3;
        r = c_sub(a, b);
        if (r.re != 3 || r.im != 4) return 5;
    }

    /* 6. 分派表: 通过函数指针调用运算符 */
    {
        int n = int_tbl.add(2, 3);
        int m = int_tbl.mul(4, 5);
        if (n != 5 || m != 20) return 6;
    }

    /* 7. 重载 + 普通运算混合 */
    {
        int n = OP_ADD(2, 3) * OP_ADD(4, 5);
        if (n != 45) return 7;   /* 5 * 9 */
    }

    /* 8. 分派验证: int vs double 实参 */
    {
        int iv = OP_ADD(5, 5);
        int dv = OP_ADD(5.0, 5.0);
        if (iv != 10) return 8;
        if (dv != 10) return 9;
    }

    /* 9. Complex 链式 */
    {
        Complex a, b, c, r;
        a.re = 1; a.im = 0;
        b.re = 2; b.im = 0;
        c.re = 3; c.im = 0;
        r = c_add(c_add(a, b), c);
        if (r.re != 6 || r.im != 0) return 10;
    }

    printf("iadd=%d dadd=%d isub=%d c=(%d,%d)\n", si, sd, di, 4, 6);
    printf("PASS\n");
    return 0;
}
