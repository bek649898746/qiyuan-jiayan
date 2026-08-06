// @EXPECTED exit:0
// @EXPECTED out:PASS
#include <stdio.h>
/* 回归: 函数指针三种调用方式 */
int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
typedef int (*binop)(int, int);

/* 回调参数方式: 函数指针作为形参 */
int apply(int (*f)(int, int), int a, int b) { return f(a, b); }

int main(void) {
    /* 方式1: 直接函数指针变量 */
    int (*fp)(int, int) = add;
    if (fp(3, 4) != 7) return 1;
    fp = sub;
    if (fp(10, 4) != 6) return 2;
    /* 方式2: typedef + 变量 */
    binop op = add;
    if (op(5, 6) != 11) return 3;
    op = sub;
    if (op(9, 2) != 7) return 4;
    /* 方式3: 函数指针作为参数 (回调) */
    if (apply(add, 2, 3) != 5) return 5;
    if (apply(sub, 10, 3) != 7) return 6;
    if (apply(op, 20, 8) != 12) return 7;
    printf("PASS\n");
    return 0;
}
