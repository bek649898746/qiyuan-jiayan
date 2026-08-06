// @EXPECTED exit:0
// @EXPECTED out:PASS
#include <stdio.h>
/* 回归: 深度递归 (100层) — 栈帧平衡 + 参数传递 */
int sum_n(int n) {
    if (n <= 0) return 0;
    return n + sum_n(n - 1);
}

int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int main(void) {
    /* 100 层递归: 1+2+...+100 = 5050 */
    if (sum_n(100) != 5050) return 1;
    /* 双递归: fib(20) = 6765 */
    if (fib(20) != 6765) return 2;
    /* 每层带多个局部变量的递归 */
    if (sum_n(50) != 1275) return 3;
    printf("PASS\n");
    return 0;
}
