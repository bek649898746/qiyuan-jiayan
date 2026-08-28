#include <stdio.h>

#define MAX(x, y) ((x) > (y) ? (x) : (y))

int main(void) {
    int a = MAX(3, 7);
    printf("a=%d (want 7)\n", a);
    if (a != 7) return 1;
    printf("PASS\n");
    return 0;
}
