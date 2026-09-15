// @EXPECTED out:x=-32768 y=65535
#include <stdio.h>
#include <stdint.h>
int main(void) {
    int16_t x = -32768;
    uint16_t y = 65535;
    printf("sizeof x=%d y=%d\n", (int)sizeof(x), (int)sizeof(y));
    printf("x=%d y=%u\n", x, y);
    return 0;
}
