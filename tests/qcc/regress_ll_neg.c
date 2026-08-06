// @EXPECTED exit:0
// @EXPECTED out:PASS
#include <stdio.h>
/* 回归: long long 负值符号扩展 (fix 2026-08-06)
   int RHS 存 64 位槽缺 movsxd → 4294967289 类零扩展错值。
   覆盖: 声明初始化/赋值/静态/逗号声明/字段/数组元素 */
static long long g = -5;
long long ga[3];

struct SF { long long v; int x; };

int main(void) {
    if (g != -5) return 1;
    long long a = -7;                 /* 声明初始化 */
    if (a != -7) return 2;
    long long c = 7, d = -3;          /* 逗号声明 (第二变量 was 存32) */
    if (c != 7 || d != -3) return 3;
    long long e; e = -9;              /* 赋值 */
    if (e != -9) return 4;
    static long long f; f = -11;
    if (f != -11) return 5;
    struct SF s; s.v = -13; s.x = 3;  /* ll 字段 */
    if (s.v != -13 || s.x != 3) return 6;
    ga[0] = -15; ga[1] = 5;           /* 全局 ll 数组元素 */
    if (ga[0] != -15 || ga[1] != 5) return 7;
    printf("PASS\n");
    return 0;
}
