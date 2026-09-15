// @EXPECTED out:PASS
#include <stdio.h>

#define TYPE_MAX(x, y) _Generic((x), \
    int: int_max, \
    long long: ll_max, \
    double: dbl_max)(x, y)

static int int_max(int a, int b) { return a > b ? a : b; }
static long long ll_max(long long a, long long b) { return a > b ? a : b; }
static double dbl_max(double a, double b) { return a > b ? a : b; }

int main(void) {
    int a = TYPE_MAX(3, 7);
    long long b = TYPE_MAX(10LL, 5LL);
    double c = TYPE_MAX(3.5, 2.5);
    printf("a=%d b=%lld c=%.1f\n", a, b, c);
    if (a != 7) return 1;
    if (b != 10) return 2;
    if (c != 3.5) return 3;
    printf("PASS\n");
    return 0;
}
