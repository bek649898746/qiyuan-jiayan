#include <stdio.h>
#include <stdlib.h>
typedef struct { int *data; int n; } Buffer;

/* 变体 B: make_buffer 返回前调 malloc + 写循环(无 printf) */
static Buffer makeB(int n) {
    Buffer b;
    b.n = n;
    b.data = (int *)malloc((size_t)n * sizeof(int));
    for (int i = 0; i < n; i++) b.data[i] = i * i;
    return b;
}
int main(void) {
    Buffer a = makeB(5);
    printf("a.n=%d a.data=%p\n", a.n, a.data);
    for (int i = 0; i < 5; i++) printf("a.data[%d]=%d\n", i, a.data[i]);
    if (!a.data || a.n != 5) return 1;
    if (a.data[4] != 16) return 2;
    printf("B PASS\n");
    return 0;
}
