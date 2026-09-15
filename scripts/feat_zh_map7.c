#include <stdio.h>
//  映射库配套 (时间词), 单文件不链
// @EXPECTED out:PASS 2026-09-15
#include <time.h>

/* 甲言 中文化映射 第7批 测试 (2026-08-28): 格式时间 */

int main(void) {
    long t = (long)取时间(0);
    struct tm *lt = 取本地时间(&t);
    char buf[64];
    unsigned n = (unsigned)格式时间(buf, 64, "%Y-%m-%d", lt);
    if (n < 8) { printf("FAIL strftime: %u %s\n", n, buf); return 1; }
    /* 年份合理 */
    if (buf[0] != '2' || buf[1] != '0') { printf("FAIL year: %s\n", buf); return 1; }
    /* R5c (2026-09-09): struct tm 字段访问 (原 sizeof=0 布局未定义 → tm_year 恒 0) */
    if (lt->tm_year + 1900 < 2020 || lt->tm_year + 1900 > 2100) { printf("FAIL tm_year: %d\n", lt->tm_year); return 2; }
    if (lt->tm_mon < 0 || lt->tm_mon > 11) { printf("FAIL tm_mon: %d\n", lt->tm_mon); return 3; }
    if (lt->tm_mday < 1 || lt->tm_mday > 31) { printf("FAIL tm_mday: %d\n", lt->tm_mday); return 4; }
    printf("PASS %s\n", buf);
    return 0;
}
