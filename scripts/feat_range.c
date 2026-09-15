// @EXPECTED out:PASS
#include <stdio.h>
#include <stdint.h>

/* 区间类型基础: C 的位宽类型 + 回绕语义 (无符号回绕是 C 标准) */
int main(void) {
    int8_t a = 127;
    int8_t b = (int8_t)(a + 1);        /* 有符号溢出: 实现定义, gcc 回绕 → -128 */
    uint8_t c = 255;
    uint8_t d = (uint8_t)(c + 1);      /* 无符号回绕 → 0 */
    uint8_t e = (uint8_t)(c + 2);      /* → 1 */

    printf("a=%d b=%d c=%u d=%u e=%u\n", a, b, c, d, e);
    if (d != 0) return 1;
    if (e != 1) return 2;

    /* 位宽裁剪: 16 位回绕 */
    uint16_t u = 65535;
    uint16_t v = (uint16_t)(u + 1);    /* → 0 */
    if (v != 0) return 3;

    /* 移位范围: char 左移 8 位 (int 提升后) */
    unsigned char x = 1;
    int y = x << 8;                    /* 提升为 int, 256 */
    if (y != 256) return 4;

    printf("PASS\n");
    return 0;
}
