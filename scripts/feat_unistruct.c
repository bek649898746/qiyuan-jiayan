// @EXPECTED out:PASS
#include <stdio.h>
typedef enum { A, B } K;
typedef struct {
    K kind;
    union {
        struct { double r; } c;
        struct { double w, h; } r;
    } u;
} S;
int main(void) {
    S s; s.kind = A; s.u.c.r = 2.0;
    double x = s.u.c.r;
    printf("x=%.4f\n", x);
    if (x != 2.0) return 1;
    S t; t.kind = B; t.u.r.w = 3.0; t.u.r.h = 4.0;
    double y = t.u.r.w * t.u.r.h;
    printf("y=%.4f\n", y);
    if (y != 12.0) return 2;
    printf("PASS\n");
    return 0;
}
