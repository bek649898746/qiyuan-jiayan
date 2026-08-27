#include <stdio.h>
typedef struct {
    int (*apply)(void *ctx, int x);
    void *ctx;
} Closure;
static int add_n(void *ctx, int x) {
    int n = *(int *)ctx;
    return x + n;
}
static void make_closure(Closure *out, int (*fn)(void *, int), void *ctx) {
    out->apply = fn;
    out->ctx = ctx;
}
int main(void) {
    int captured = 10;
    Closure c;
    make_closure(&c, add_n, &captured);
    int a = c.apply(c.ctx, 5);
    printf("a=%d\n", a);
    if (a != 15) return 1;
    printf("PASS\n");
    return 0;
}
