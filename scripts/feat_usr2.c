#include <stdio.h>
typedef struct {
    union {
        struct { double w, h; } r;
    } u;
} S;
int main(void) {
    S t; t.u.r.w = 3.0;
    double w = t.u.r.w;
    printf("w=%.4f\n", w);
    if (w != 3.0) return 1;
    S t2; t2.u.r.h = 4.0;
    double h = t2.u.r.h;
    printf("h=%.4f\n", h);
    if (h != 4.0) return 2;
    printf("PASS\n");
    return 0;
}
