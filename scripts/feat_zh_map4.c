#include <stdio.h>
//  映射库配套 (时间词), 单文件不链
// @EXPECTED out:PASS
#include <time.h>
#include <string.h>

/* 甲言 中文化映射 第4批 测试 (2026-08-28): 求时间差/取本地时间/取格林威治/绝对长整/找字符集/转换浮点串 */

int main(void) {
    time_t t1 = 取时间(0);
    time_t t2 = t1 + 1;
    double d = 求时间差(t2, t1);
    if (d != 1.0) { printf("FAIL difftime: %f\n", d); return 1; }
    struct tm *lt = 取本地时间(&t1);
    if (lt == 0) { printf("FAIL localtime\n"); return 1; }
    struct tm *gt = 取格林威治(&t1);
    if (gt == 0) { printf("FAIL gmtime\n"); return 1; }
    if (绝对长整(-42L) != 42L) { printf("FAIL labs\n"); return 1; }
    if (找字符集("hello", "ol") == 0) { printf("FAIL strpbrk\n"); return 1; }
    double f = 转换浮点串("3.14", 0);
    if (f < 3.13 || f > 3.15) { printf("FAIL strtod: %f\n", f); return 1; }
    printf("PASS\n");
    return 0;
}
