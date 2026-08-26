#include <stdio.h>
#include <stdint.h>
typedef long long ll2;
int main(void) {
    int64_t v1 = -9223372036854775807LL - 1;
    uint64_t v2 = 18446744073709551615ULL;
    ll2 v3 = -9223372036854775807LL - 1;
    long long v4 = -9223372036854775807LL - 1;
    printf("sizeof: int64_t=%d uint64_t=%d ll2=%d longlong=%d\n",
        (int)sizeof(int64_t), (int)sizeof(uint64_t), (int)sizeof(ll2), (int)sizeof(long long));
    printf("v1=%lld v2=%llu v3=%lld v4=%lld\n", v1, v2, v3, v4);
    return 0;
}
