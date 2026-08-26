#include <stdio.h>
int main(void) {
    long long a = -9223372036854775807LL - 1;
    long long b = -9223372036854775807LL;
    long long c = 9223372036854775807LL;
    long long d = -1LL;
    printf("a=%lld b=%lld c=%lld d=%lld\n", a, b, c, d);
    int i32 = -2147483648;
    long long wide1 = (long long)i32 * 1000000LL;
    long long wide2 = i32 * 1000000LL;
    long long wide3 = 1000000LL * i32;
    printf("wide1=%lld wide2=%lld wide3=%lld\n", wide1, wide2, wide3);
    long long e = 4294967296LL;
    long long f2 = 2147483648LL;
    printf("e=%lld f2=%lld\n", e, f2);
    return 0;
}
