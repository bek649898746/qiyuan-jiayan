// @EXPECTED out:PASS
#include <stdio.h>

/* 闭包模拟: 函数指针 + 环境捕获 (C 的 ctx 约定, 无 RTTI) */
typedef struct {
    int (*apply)(void *ctx, int x);
    void *ctx;
} Closure;

static int add_n(void *ctx, int x) {
    int n = *(int *)ctx;
    return x + n;
}
static int mul_n(void *ctx, int x) {
    int n = *(int *)ctx;
    return x * n;
}
static int neg(void *ctx, int x) {
    (void)ctx;
    return -x;
}

static Closure make_closure(int (*fn)(void *, int), void *ctx) {
    Closure c;
    c.apply = fn;
    c.ctx = ctx;
    return c;
}

int main(void) {
    int captured_add = 10;
    int captured_mul = 3;

    Closure add10 = make_closure(add_n, &captured_add);
    Closure mul3 = make_closure(mul_n, &captured_mul);
    Closure negc = make_closure(neg, NULL);

    int a = add10.apply(add10.ctx, 5);   /* 5 + 10 = 15 */
    int b = mul3.apply(mul3.ctx, 7);     /* 7 * 3 = 21 */
    int c = negc.apply(negc.ctx, 4);     /* -4 */

    printf("a=%d b=%d c=%d\n", a, b, c);
    if (a != 15) return 1;
    if (b != 21) return 2;
    if (c != -4) return 3;
    printf("PASS\n");
    return 0;
}
