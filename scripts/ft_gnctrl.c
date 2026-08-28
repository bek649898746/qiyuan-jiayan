#include <stdio.h>

int main(void) {
    int a = 3 > 7 ? 3 : 7;
    printf("a=%d (want 7)\n", a);
    if (a != 7) return 1;
    printf("PASS\n");
    return 0;
}
