#include <stdio.h>
#include <stdint.h>
int main(void) {
    int16_t x = -32768;
    uint16_t y = 65535;
    int s1 = sizeof(x);
    int s2 = sizeof(y);
    int s3 = sizeof(int16_t);
    printf("s1=%d s2=%d s3=%d\n", s1, s2, s3);
    if (s1 != 2) return 1;
    if (s2 != 2) return 2;
    if (s3 != 2) return 3;
    printf("PASS\n");
    return 0;
}
