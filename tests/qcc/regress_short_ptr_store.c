// @EXPECTED exit:0
// 回归: 指针变量按元素宽度存储 (case-10 pe 分支, fix 2026-08-09)
// 覆盖: 整*写4字节 / 字节*写1 / 短*写2 / 无短*写2 — 且不污染相邻字节
#include <stdio.h>
int main(void) {
    char buf[16];
    int i;
    int *ip;
    char *cp;
    short *sp;
    unsigned short *usp;
    for (i = 0; i < 16; i++) buf[i] = (char)(i + 1);

    ip = (int*)&buf[0];
    *ip = 0x78563412;
    if (buf[0] != 0x12 || buf[1] != 0x34 || buf[2] != 0x56 || buf[3] != 0x78) return 1;

    cp = &buf[4];
    *cp = 0x7F;
    if (buf[4] != 0x7F || buf[5] != 6) return 2;

    sp = (short*)&buf[6];
    *sp = 0xABCD;
    if (buf[6] != 0xCD || buf[7] != 0xAB) return 3;
    if (buf[8] != 9) return 4;   /* 2 字节写不得污染 b8 */

    usp = (unsigned short*)&buf[8];
    *usp = 0x1234;
    if (buf[8] != 0x34 || buf[9] != 0x12) return 5;
    if (buf[10] != 11) return 6; /* 2 字节写不得污染 b10 */

    printf("shortstore-ok\n");
    return 0;
}
