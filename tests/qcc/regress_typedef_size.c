// @EXPECTED exit:0
// @EXPECTED out:8-8
#include <stdio.h>

typedef int myint;
typedef unsigned long long ull;

struct A {
    myint a;
    char *b;
};

struct B {
    ull a;
    char *b;
};

int main(void) {
    printf("%d-%d\n",
           (int)((char *)&((struct A *)0)->b - (char *)((struct A *)0)),
           (int)((char *)&((struct B *)0)->b - (char *)((struct B *)0)));
    return 0;
}
