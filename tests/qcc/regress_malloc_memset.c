// @EXPECTED exit:0
// @EXPECTED out:000000
// 测试 qcc 的 malloc + memset (v1 下)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    unsigned char *p = (unsigned char*)malloc(12);
    memset(p, 0, 12);
    printf("%02x%02x%02x%02x%02x%02x\n", p[0], p[4], p[8], p[11], p[1], p[2]);
    return 0;
}
