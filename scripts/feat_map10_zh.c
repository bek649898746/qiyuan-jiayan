#include <stdio.h>
#include <ctype.h>

/* 甲言 映射第10批 接入测试 (2026-08-28): bn 内置 格式化 IO/判类 中文映射 */

int main(void) {
    /* 格式化打印 → printf */
    格式化打印("F%d\n", 7);
    /* 输出到文件 → fprintf */
    FILE *f = 打开文件("fm10.txt", "w");
    if (!f) { printf("FAIL 打开文件\n"); return 1; }
    输出到文件(f, "x=%d\n", 42);
    关闭文件(f);
    /* 判文件错误 → ferror (句柄语义恒 0) */
    f = 打开文件("fm10.txt", "r");
    if (!f) { printf("FAIL 打开文件 r\n"); return 1; }
    if (判文件错误(f) != 0) { printf("FAIL 判文件错误\n"); return 1; }
    关闭文件(f);
    /* 判字母数字 → isalnum */
    if (!判字母数字('a') || !判字母数字('5')) { printf("FAIL 判字母数字1\n"); return 1; }
    if (判字母数字('!')) { printf("FAIL 判字母数字2\n"); return 1; }
    /* 读格式 → scanf (标准输入) */
    int v = 0;
    if (读格式("%d", &v) != 1) { printf("FAIL 读格式\n"); return 1; }
    if (v != 42) { printf("FAIL 读格式 val: %d\n", v); return 1; }
    格式化打印("PASS\n");
    return 0;
}
