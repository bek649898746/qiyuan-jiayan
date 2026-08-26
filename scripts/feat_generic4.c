#include <stdio.h>
static int int_max(int a, int b) { return a > b ? a : b; }
static long long ll_max(long long a, long long b) { return a > b ? a : b; }
int main(void) {
    int (*fp)(int, int) = int_max;
    int a = _Generic((3), int: fp, long long: ll_max)(3, 7);
    printf("a=%d\n", a);
    if (a != 7) return 1;
    int b = _Generic((5), int: int_max, long long: ll_max)(5, 9);
    printf("b=%d\n", b);
    if (b != 9) return 2;
    printf("PASS\n");
    return 0;
}
