#include <stdio.h>
struct U { int ok; union { int val; const char *err; } u; };
static struct U tp(const char *s) {
    struct U u;
    u.ok = 1;
    u.u.val = s[0] - '0';
    return u;
}
int main(void) {
    const char *msg = "7";
    struct U u = tp(msg);
    printf("ok=%d val=%d\n", u.ok, u.u.val);
    if (u.ok != 1 || u.u.val != 7) return 2;
    printf("PASS\n");
    return 0;
}
