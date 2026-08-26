#include <stdio.h>

static int int_max(int a, int b) { return a > b ? a : b; }
static long long ll_max(long long a, long long b) { return a > b ? a : b; }
static double dbl_max(double a, double b) { return a > b ? a : b; }

int main(void) {
    int a = _Generic((3), int: int_max, long long: ll_max, double: dbl_max)(3, 7);
    long long b = _Generic((10LL), int: int_max, long long: ll_max, double: dbl_max)(10LL, 5LL);
    printf("a=%d b=%lld\n", a, b);
    if (a != 7) return 1;
    if (b != 10) return 2;
    printf("PASS\n");
    return 0;
}
