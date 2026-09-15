// @EXPECTED out:PASS
#include <stdio.h>
struct S { double d; int x; };
struct T { int x; double d; };
int main(void) {
    struct S s; s.d = 3.14; s.x = 7;
    struct T t; t.x = 9; t.d = 2.71;
    printf("s.d=%.2f s.x=%d t.x=%d t.d=%.2f\n", s.d, s.x, t.x, t.d);
    printf("sizeof S=%d T=%d\n", (int)sizeof(struct S), (int)sizeof(struct T));
    if (s.d != 3.14) return 1;
    if (s.x != 7) return 2;
    if (t.d != 2.71) return 3;
    printf("PASS\n");
    return 0;
}
