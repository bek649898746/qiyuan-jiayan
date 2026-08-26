#include <stdio.h>
#include <stdint.h>
int main(void) {
    long long a = INT64_MAX;
    long long b = INT64_MIN;
    printf("INT64_MAX=%lld INT64_MIN=%lld\n", a, b);
    return 0;
}
