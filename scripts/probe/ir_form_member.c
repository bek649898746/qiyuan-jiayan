#include <stdio.h>

struct S { int a; int b; };

int main(void) {
    struct S s;
    s.a = 3;
    s.b = 4;
    printf("%d %d\n", s.a, s.b);
    return 0;
}
