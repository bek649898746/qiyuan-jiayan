#include <stdio.h>
// @DEP 映射库配套 (时间词), 单文件不链
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
    printf("PASS %s\n", buf);
    return 0;
}
