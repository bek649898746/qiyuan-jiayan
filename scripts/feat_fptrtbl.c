// @EXPECTED out:PASS
#include <stdio.h>
typedef struct { double r; } Circle;
static double circle_area(const void *self) {
    const Circle *c = (const Circle *)self;
    return c->r * c->r;
}
typedef double (*AreaFn)(const void *);
int main(void) {
    Circle c = { 2.0 };
    double direct = circle_area(&c);
    printf("direct=%.4f\n", direct);
    if (direct != 4.0) return 1;
    AreaFn fn = circle_area;
    double via_fp = fn(&c);
    printf("via_fp=%.4f\n", via_fp);
    if (via_fp != 4.0) return 2;
    printf("PASS\n");
    return 0;
}
