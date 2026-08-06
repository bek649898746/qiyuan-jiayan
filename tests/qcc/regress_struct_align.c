// @EXPECTED exit:0
// @EXPECTED out:PASS
#include <stdio.h>
/* 回归: 结构体对齐/填充字节
   SA{char,int}:          c@0 i@4 sizeof=8   (M2 fix: char+int 应 8 非 5)
   SB{char,char,short,int}: a@0 b@1 s@2 i@4 sizeof=8 (标准 ABI; short 词法分类修复 2026-08-06)
   SD{double,int}:        d@0 i@8 sizeof=16  (尾部填充 round up fix 2026-08-06: 原 12 未按 algn 取整) */
struct SA { char c; int i; };
struct SB { char a; char b; short s; int i; };
struct SD { double d; int i; };
static struct SA gsa;   /* 静态结构体成员 */

int main(void) {
    struct SA sa; struct SB sb; struct SD sd;
    /* 偏移量 */
    if ((int)((char*)&sa.i - (char*)&sa) != 4) return 1;
    if ((int)sizeof(struct SA) != 8) return 2;
    if ((int)((char*)&sb.b - (char*)&sb) != 1) return 3;
    if ((int)((char*)&sb.s - (char*)&sb) != 2) return 4;
    if ((int)((char*)&sb.i - (char*)&sb) != 4) return 5;
    if ((int)sizeof(struct SB) != 8) return 6;
    if ((int)((char*)&sd.i - (char*)&sd) != 8) return 7;
    if ((int)sizeof(struct SD) != 16) return 8;
    /* 字段写读回 (char/int/short 已支持宽度 — short 16 位存取 fix 2026-08-06) */
    sa.c = 1; sa.i = 0x12345678;
    if (sa.c != 1 || sa.i != 0x12345678) return 9;
    sb.a = 9; sb.b = 8; sb.s = 300; sb.i = 0xDEAD;
    if (sb.a != 9 || sb.b != 8 || sb.s != 300 || sb.i != 0xDEAD) return 10;
    sd.i = 7; if (sd.i != 7) return 11;
    /* short 字段: 写短值不踩相邻字段 (原 32 位存取踩 i) */
    sb.s = 0x7FFF; if (sb.i != 0xDEAD) return 15;
    /* 结构体数组: 元素 stride = sizeof */
    struct SA arr[3];
    arr[0].i = 10; arr[1].i = 20; arr[2].i = 30;
    arr[0].c = 1; arr[2].c = 3;
    if (arr[1].i != 20) return 12;
    if ((int)((char*)&arr[1] - (char*)&arr[0]) != 8) return 13;
    /* 静态结构体成员写读 */
    gsa.c = 5; gsa.i = 6;
    if (gsa.c != 5 || gsa.i != 6) return 14;
    printf("PASS\n");
    return 0;
}
