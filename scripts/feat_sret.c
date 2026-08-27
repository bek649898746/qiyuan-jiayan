#include <stdio.h>
struct R { int ok; int val; };
static struct R f(void) {
    struct R r;
    r.ok = 1;
    r.val = 7;
    return r;
}
struct U { int ok; union { int val; const char *err; } u; };
static struct U g(void) {
    struct U u;
    u.ok = 1;
    u.u.val = 7;
    return u;
}
int main(void) {
    struct R r = f();
    printf("r.ok=%d r.val=%d\n", r.ok, r.val);
    if (r.ok != 1 || r.val != 7) return 1;
    struct U u = g();
    printf("u.ok=%d u.val=%d\n", u.ok, u.u.val);
    if (u.ok != 1 || u.u.val != 7) return 2;
    printf("PASS\n");
    return 0;
}
