#include <stdio.h>
int main(void) {
    int x = 5;
    long long y = 10LL;
    int a = _Generic((x), int: 100, long long: 200, default: 300);
    long long b = _Generic((y), int: 100, long long: 200, default: 300);
    double d = 3.5;
    int c = _Generic((d), int: 100, double: 200, default: 300);
    printf("a=%d b=%lld c=%d\n", a, b, c);
    if (a != 100) return 1;
    if (b != 200) return 2;
    if (c != 200) return 3;
    printf("PASS\n");
    return 0;
}
