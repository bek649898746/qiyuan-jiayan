#include <stdio.h>
typedef struct { int *data; int n; } Buffer;
static int gbuf[8];
int main(void) {
    Buffer a;
    a.n = 5;
    a.data = gbuf;
    a.data[0] = 42;
    a.data[1] = 7;
    a.data[2] = 9;
    printf("gbuf=%d,%d,%d\n", gbuf[0], gbuf[1], gbuf[2]);
    printf("read=%d,%d,%d\n", a.data[0], a.data[1], a.data[2]);
    if (a.data[0] != 42 || a.data[1] != 7 || a.data[2] != 9) return 2;
    if (gbuf[0] != 42 || gbuf[1] != 7 || gbuf[2] != 9) return 3;
    printf("F PASS\n");
    return 0;
}
