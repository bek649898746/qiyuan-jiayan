// @EXPECTED out:PASS
#include <stdio.h>
#include <string.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef const int ci;
typedef int (*fp)(int, int);
typedef struct Pair { int a; int b; } Pair;
typedef Pair PairArr[2];
typedef int i4;

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }

int main(void) {
    u8 u = 255;
    u16 uw = 65535;
    ci c = 7;
    fp fns[2] = { add, sub };
    Pair pairs[2] = { {1,2}, {3,4} };
    PairArr pa = { {5,6}, {7,8} };
    i4 v = 99;

    printf("u=%d uw=%u c=%d\n", u, uw, c);
    printf("fns: %d %d\n", fns[0](5,3), fns[1](5,3));
    printf("pairs: %d,%d %d,%d\n", pairs[0].a, pairs[0].b, pairs[1].a, pairs[1].b);
    printf("pa: %d,%d %d,%d\n", pa[0].a, pa[0].b, pa[1].a, pa[1].b);
    printf("v=%d\n", v);
    printf("sizeof: u8=%d u16=%d ci=%d fp=%d Pair=%d PairArr=%d fns=%d pairs=%d pa=%d i4=%d\n",
        (int)sizeof(u8), (int)sizeof(u16), (int)sizeof(ci), (int)sizeof(fp),
        (int)sizeof(Pair), (int)sizeof(PairArr), (int)sizeof(fns), (int)sizeof(pairs),
        (int)sizeof(pa), (int)sizeof(i4));

    if (u != 255) return 1;
    if (uw != 65535) return 2;
    if (c != 7) return 3;
    if (fns[0](5,3) != 8 || fns[1](5,3) != 2) return 4;
    if (pairs[1].a != 3) return 5;
    if (pa[0].b != 6) return 6;
    if (v != 99) return 7;
    if (sizeof(u8) != 1) return 8;
    if (sizeof(u16) != 2) return 9;
    if (sizeof(Pair) != 8) return 10;
    if (sizeof(PairArr) != 16) return 11;
    if (sizeof(fns) != 16) return 12;
    if (sizeof(pairs) != 16) return 13;
    if (sizeof(pa) != 16) return 14;
    printf("PASS\n");
    return 0;
}
