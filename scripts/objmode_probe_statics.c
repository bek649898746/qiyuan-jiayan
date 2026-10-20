#include <stdio.h>
static int g1 = 42;
static long g2 = -7;
static const char *s1 = "hello";
static int arr[4] = {1, 2, 3, 4};
static int *p1 = &g1;
static int (*fp)(const char *, ...) = printf;
int main(void) {
    printf("%d %ld %s %d %d\n", g1, g2, s1, arr[2], *p1);
    fp("fp\n");
    return 0;
}
