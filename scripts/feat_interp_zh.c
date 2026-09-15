// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 字符串插值 (格式化 "x={} y={}", x, y) 接入测试 (2026-08-27):
 *   格式化 "fmt", args...; → printf(新格式串, args...)
 *   {} 占位符按参数类型自动转格式符: int→%d, unsigned→%u, double→%f, ll→%lld, char→%c, ptr→%s
 */

int main(void) {
    /* ① int + double + 字符串 */
    int a = 42;
    double d = 3.5;
    const char *s = "hello";
    格式化 "a={} d={} s={}", a, d, s;
    printf("\n");   /* 换行 */

    /* ② 混合文本 */
    int x = 7;
    格式化 "value={} squared={}", x, x * x;
    printf("\n");

    /* ③ unsigned + char */
    unsigned u = 255;
    char c = 'A';
    格式化 "u={} c={}", u, c;
    printf("\n");

    /* ④ 与已有特性组合 */
    int y = 10;
    夹取 y, 0, 5;   /* 10 → 5 */
    格式化 "clamped={}", y;
    printf("\n");

    printf("PASS\n");
    return 0;
}
