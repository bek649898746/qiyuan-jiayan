// @EXPECTED out:PASS
#include <stdio.h>
#include <string.h>

/* 甲言 映射第9批 接入测试 (2026-08-28): bn 内置文件 IO 中文映射 + feof r10-volatile bug 修复 */

int main(void) {
    /* 打开文件 w + 写行 + 关闭 */
    FILE *f = 打开文件("fm9.txt", "w");
    if (!f) { printf("FAIL 打开文件 w\n"); return 1; }
    fputs("hello", f);
    fclose(f);
    /* 打开文件 r + 读字符 */
    f = 打开文件("fm9.txt", "r");
    if (!f) { printf("FAIL 打开文件 r\n"); return 1; }
    int c = 读字符(f);
    if (c != 'h') { printf("FAIL 读字符1: %c\n", c); return 1; }
    c = 读字符(f);
    if (c != 'e') { printf("FAIL 读字符2: %c\n", c); return 1; }
    /* 判文件结束 (位置比较: 读完 'o' 后置位) */
    char buf[16]; int n = 0;
    while (n < 10) { c = 读字符(f); if (判文件结束(f)) break; buf[n++] = c; }
    buf[n] = 0;
    if (strcmp(buf, "ll") != 0) { printf("FAIL rest: %s\n", buf); return 1; }
    /* 取文件位置 */
    long pos = 取文件位置(f);
    if (pos != 5) { printf("FAIL 取文件位置: %ld\n", pos); return 1; }
    /* 倒带 */
    倒带(f);
    c = 读字符(f);
    if (c != 'h') { printf("FAIL 倒带: %c\n", c); return 1; }
    /* 定位文件 */
    定位文件(f, 2, 0);
    c = 读字符(f);
    if (c != 'l') { printf("FAIL 定位文件: %c\n", c); return 1; }
    /* 写字符 + 刷新缓冲 */
    fclose(f);
    f = 打开文件("fm9.txt", "a");
    if (!f) { printf("FAIL 打开文件 a\n"); return 1; }
    写字符('!', f);
    刷新缓冲(f);
    fclose(f);
    f = 打开文件("fm9.txt", "r");
    if (!f) { printf("FAIL 打开文件 r2\n"); return 1; }
    定位文件(f, 5, 0);
    c = 读字符(f);
    if (c != '!') { printf("FAIL 刷新缓冲: %c\n", c); return 1; }
    fclose(f);
    printf("PASS\n");
    return 0;
}
