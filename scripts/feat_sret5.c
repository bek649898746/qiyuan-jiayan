#include <stdio.h>
struct U { int ok; union { int val; const char *err; } u; };
static struct U tp(const char *s) {
    struct U u;
    u.ok = 1;
    u.u.val = s[0] - '0';
    return u;
}
int main(void) {
    struct U u = tp("7");
    printf("u.ok=%d u.val=%d\n", u.ok, u.u.val);
    if (u.ok != 1 || u.u.val != 7) return 2;
    printf("PASS\n");
    return 0;
}
