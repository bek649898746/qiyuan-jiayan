#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 甲言 中文化映射 第2批 测试 (2026-08-28): 字符串/数学/内存 12 个新映射 */

int main(void) {
    /* 字符串: 拷贝前n / 拼前n / 比前n */
    char s1[32] = "hello";
    char s2[32] = "world";
    char buf[64];
    拷贝前n(buf, s1, 3); buf[3] = 0;
    if (字比(buf, "hel") != 0) { printf("FAIL strncpy\n"); return 1; }
    拼前n(buf, s2, 2);
    if (字比(buf, "helwo") != 0) { printf("FAIL strncat: %s\n", buf); return 1; }
    if (比前n("abc", "abd", 2) != 0) { printf("FAIL strncmp\n"); return 1; }
    /* 查字符 / 反向查 */
    if (查字符("hello", 'l') == 0) { printf("FAIL strchr\n"); return 1; }
    if (反向查("hello", 'l') == 0) { printf("FAIL strrchr\n"); return 1; }
    if (取限长("abc", 2) != 2) { printf("FAIL strnlen\n"); return 1; }
    /* 拆分 (可写数组, strtok 会修改) */
    char s[] = "a,b,c";
    char *tk = 拆分(s, ",");
    if (tk == 0 || 字比(tk, "a") != 0) { printf("FAIL strtok\n"); return 1; }
    /* 字符串转长整 / 转无符号 */
    if (字符串转长整("123", 0, 10) != 123) { printf("FAIL strtol\n"); return 1; }
    if (字符串转无符号("456", 0, 10) != 456) { printf("FAIL strtoul\n"); return 1; }
    /* 数学: 四舍五入 / 截断 */
    if (四舍五入(2.5) != 3.0) { printf("FAIL round\n"); return 1; }
    if (截断(2.9) != 2.0) { printf("FAIL trunc\n"); return 1; }
    /* 内存: 重分配 */
    int *p = (int*)分配(8);
    p = (int*)重分配(p, 16);
    p[0] = 42;
    if (p[0] != 42) { printf("FAIL realloc\n"); return 1; }
    释放(p);
    printf("PASS\n");
    return 0;
}
