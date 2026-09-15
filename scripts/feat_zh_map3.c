// @EXPECTED out:PASS
#include <stdio.h>
#include <time.h>

/* 甲言 中文化映射 第3批 测试 (2026-08-28): 取时间/时钟 */

int main(void) {
    long t = (long)取时间(0);
    printf("time=%ld\n", t);
    if (t < 1000000000L) { printf("FAIL time\n"); return 1; }
    clock_t c = 时钟();
    printf("clock=%ld\n", (long)c);
    if (c < 0) { printf("FAIL clock\n"); return 1; }
    printf("PASS\n");
    return 0;
}
