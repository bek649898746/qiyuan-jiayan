/* gcc 神谕 对照 — 与 feat_zh_type_alias_width_ok.c **同逻辑** 的 C 写法。
   用法: gcc -O0 -o _oracle.exe feat_zh_type_alias_width_ok_oracle.c && ./_oracle.exe
   要求: 输出 `PASS` 且 exit 0 —— 与甲言锚 逐值一致。 */
#include <stdio.h>

typedef unsigned char 字节型;
typedef unsigned int 无整型;
typedef unsigned 纯无型;
typedef int 整数型;
typedef short 短型;
typedef long long 长型;
typedef double 双型;
typedef signed char 有符字;

typedef int 甲型;
typedef 甲型 乙型;

int main(void) {
    字节型 b = 200;
    if (b != 200) return 1;
    无整型 u = 200;
    if (u != 200) return 2;
    纯无型 v = 200;
    if (v != 200) return 3;
    有符字 c = 100;          /* ⚠️ signed char ⟹ 只能 到 127 */
    if (c != 100) return 4;

    if (sizeof(字节型) != 1) return 10;
    if (sizeof(有符字) != 1) return 11;
    if (sizeof(短型) != 2) return 12;
    if (sizeof(整数型) != 4) return 13;
    if (sizeof(无整型) != 4) return 14;
    if (sizeof(纯无型) != 4) return 15;
    if (sizeof(长型) != 8) return 16;
    if (sizeof(双型) != 8) return 17;

    整数型 n = 42;
    if (n != 42) return 20;
    短型 s = 300;
    if (s != 300) return 21;
    长型 l = 10000000000LL;
    if (l != 10000000000LL) return 22;
    双型 d = 3.5;
    if (d != 3.5) return 23;

    乙型 e = 200;
    if (e != 200) return 24;
    if (sizeof(乙型) != 4) return 25;

    printf("PASS\n");
    return 0;
}
