#include <stdio.h>
typedef struct { int *data; int n; } Buffer;
static Buffer make2(int n) {
    Buffer b;
    b.n = n;
    b.data = (int *)0;
    return b;
}
int main(void) {
    Buffer a = make2(5);
    printf("a.n=%d a.data=%p\n", a.n, a.data);
    if (a.n != 5) return 1;
    if (a.data != (int *)0) return 2;
    printf("PASS\n");
    return 0;
}
