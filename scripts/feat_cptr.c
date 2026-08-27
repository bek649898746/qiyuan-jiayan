#include <stdio.h>
typedef struct { double r; } Circle;
static double circle_area2(const Circle *c) { return c->r * c->r; }
int main(void) {
    Circle c = { 2.0 };
    double d = circle_area2(&c);
    printf("d=%.4f\n", d);
    if (d != 4.0) return 1;
    double x = c.r;
    printf("x=%.4f\n", x);
    if (x != 2.0) return 2;
    printf("PASS\n");
    return 0;
}
