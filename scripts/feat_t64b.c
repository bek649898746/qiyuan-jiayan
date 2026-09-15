// @EXPECTED out:v1=-9223372036854775808 v2=18446744073709551615
#include <stdio.h>
typedef long long int64_t;
typedef unsigned long long uint64_t;
int main(void) {
    printf("sizeof self int64_t=%d uint64_t=%d\n", (int)sizeof(int64_t), (int)sizeof(uint64_t));
    int64_t v1 = -9223372036854775807LL - 1;
    uint64_t v2 = 18446744073709551615ULL;
    printf("v1=%lld v2=%llu\n", v1, v2);
    return 0;
}
