// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 完全中文化: 常用库函数中文映射测试 (2026-08-27):
 *   退出→exit, 关文件→fclose, 取字符→getchar, 放字符→putchar,
 *   转整数→atoi, 转长整→atol, 转浮点→atof, 绝对值→abs, 读行→fgets, 写行→fputs
 */

int main(void) {
    /* ① 转整数 */
    int n = 转整数("42");   /* atoi("42") */
    if (n != 42) return 1;

    /* ② 转浮点 */
    double d = 转浮点("3.5");  /* atof("3.5") */
    if (d != 3.5) return 2;

    /* ③ 绝对值 */
    int a = 绝对值(-7);   /* abs(-7) */
    if (a != 7) return 3;

    /* ④ 写行到 stdout (fputs 需 stdout 参数) */
    写行("hello\n", stdout);   /* fputs("hello\n", stdout) */

    /* ⑤ 与已有特性组合 */
    夹取 n, 0, 100;
    格式化 "n={}", n;
    printf("\n");

    printf("PASS\n");
    return 0;
}
