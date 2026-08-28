#include <stdio.h>

泛型 最大值(x, y) = ((x) > (y) ? (x) : (y));

int main(void) {
    int a = 最大值(3, 7);
    printf("a=%d (want 7)\n", a);
    if (a != 7) return 1;
    printf("PASS\n");
    return 0;
}
