#include <stdio.h>
typedef struct { union { int i; double d; } val; } V;
typedef struct { int k; union { int i; double d; } val; } V2;
int main(void) {
    V v; v.val.d = 3.14;
    double x = v.val.d;
    printf("x=%.2f\n", x);
    V2 v2; v2.k = 1; v2.val.d = 2.71;
    double y = v2.val.d;
    printf("y=%.2f\n", y);
    if (x != 3.14) return 1;
    if (y != 2.71) return 2;
    printf("PASS\n");
    return 0;
}
