#include <stdio.h>
static int int_max(int a, int b) { return a > b ? a : b; }
int main(void) {
    int (*fp)(int, int) = int_max;
    int a = fp(3, 7);
    printf("a=%d\n", a);
    if (a != 7) return 1;
    int b = int_max(5, 9);
    printf("b=%d\n", b);
    if (b != 9) return 2;
    printf("PASS\n");
    return 0;
}
