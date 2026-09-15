// @EXPECTED out:PASS
#include <stdio.h>
struct U { int ok; union { int val; const char *err; } u; };
static struct U g(int x) {
    struct U u;
    u.ok = x;
    u.u.val = 7;
    return u;
}
int main(void) {
    struct U u = g(1);
    printf("u.ok=%d u.val=%d\n", u.ok, u.u.val);
    if (u.ok != 1 || u.u.val != 7) return 2;
    printf("PASS\n");
    return 0;
}
