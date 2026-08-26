#include <stdio.h>
typedef signed char s8;
typedef long long ll;
typedef float ff;
typedef unsigned long long ull;
int main(void) {
    printf("sizes: char=%d short=%d int=%d long=%d llong=%d float=%d double=%d ptr=%d\n",
        (int)sizeof(char), (int)sizeof(short), (int)sizeof(int), (int)sizeof(long),
        (int)sizeof(long long), (int)sizeof(float), (int)sizeof(double), (int)sizeof(int*));
    printf("sizes typedef: s8=%d ll=%d ff=%d ull=%d\n",
        (int)sizeof(s8), (int)sizeof(ll), (int)sizeof(ff), (int)sizeof(ull));
    printf("sizes expr: sizeof(1LL)=%d sizeof(1.0f)=%d sizeof(1.5)=%d\n",
        (int)sizeof(1LL), (int)sizeof(1.0f), (int)sizeof(1.5));
    long long a = 0x1234567890LL;
    unsigned long long b = 0xFFFFFFFFFFFFFFFFULL;
    printf("a=%lld b=%llu\n", a, b);
    long long c = 1000000000000LL;
    printf("c=%lld\n", c);
    float f = 3.5f;
    printf("f=%f\n", f);
    return 0;
}
