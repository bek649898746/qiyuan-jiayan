#include <stdio.h>

/* 甲言 映射第8批 接入测试 (2026-08-28): bn 内置内存/字符串/程序控制函数中文映射 */

int main(void) {
    /* 内存设置 → memset */
    char buf[16];
    内存设置(buf, 'A', 8); buf[8] = 0;
    if (比较字符串(buf, "AAAAAAAA") != 0) { printf("FAIL 内存设置: %s\n", buf); return 1; }
    /* 复制内存 → memcpy */
    char src[8] = "hello";
    char dst[8];
    复制内存(dst, src, 6);
    if (比较字符串(dst, "hello") != 0) { printf("FAIL 复制内存: %s\n", dst); return 1; }
    /* 内存分配 → malloc + 释放内存 → free */
    int *p = 内存分配(4 * sizeof(int));
    if (!p) { printf("FAIL 内存分配\n"); return 1; }
    p[0] = 42; p[3] = 99;
    if (p[0] != 42 || p[3] != 99) { printf("FAIL malloc wr: %d %d\n", p[0], p[3]); return 1; }
    释放内存(p);
    /* 清零分配 → calloc */
    int *q = 清零分配(4, sizeof(int));
    if (!q) { printf("FAIL 清零分配\n"); return 1; }
    if (q[0] != 0 || q[3] != 0) { printf("FAIL calloc zero: %d %d\n", q[0], q[3]); return 1; }
    q[1] = 7;
    if (q[1] != 7) { printf("FAIL calloc wr\n"); return 1; }
    释放内存(q);
    /* 比较字符串 → strcmp */
    if (比较字符串("abc", "abc") != 0) { printf("FAIL strcmp eq\n"); return 1; }
    if (比较字符串("abc", "abd") >= 0) { printf("FAIL strcmp lt\n"); return 1; }
    if (比较字符串("abd", "abc") <= 0) { printf("FAIL strcmp gt\n"); return 1; }
    /* 复制字符串 → strcpy */
    char s2[8];
    复制字符串(s2, "xyz");
    if (比较字符串(s2, "xyz") != 0) { printf("FAIL 复制字符串: %s\n", s2); return 1; }
    printf("PASS\n");
    return 0;
}
