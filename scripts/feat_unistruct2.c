#include <stdio.h>
struct Inner { double r; };
typedef struct {
    union {
        struct Inner c;
        int i;
    } u;
} U;
int main(void) {
    U u; u.u.c.r = 2.0;
    double x = u.u.c.r;
    printf("x=%.4f\n", x);
    if (x != 2.0) return 1;
    u.u.i = 7;
    if (u.u.i != 7) return 2;
    printf("PASS\n");
    return 0;
}
