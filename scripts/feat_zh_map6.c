#include <stdio.h>
//  映射库配套 (时间词), 单文件不链
// @EXPECTED out:PASS
#include <time.h>
#include <string.h>

/* 甲言 中文化映射 第6批 测试 (2026-08-28): 转时间串/转时间串2/区域比较/区域变换 */

int main(void) {
    long t = (long)取时间(0);
    char *a = 转时间串(取本地时间(&t));
    if (a == 0 || a[0] == 0) { printf("FAIL asctime\n"); return 1; }
    char *c = 转时间串2(&t);
    if (c == 0 || c[0] == 0) { printf("FAIL ctime\n"); return 1; }
    if (区域比较("abc", "abd") >= 0) { printf("FAIL strcoll\n"); return 1; }
    char buf[32];
    unsigned n = (unsigned)区域变换(buf, "hello", 5);
    if (n != 5) { printf("FAIL strxfrm: %u\n", n); return 1; }
    printf("PASS\n");
    return 0;
}
