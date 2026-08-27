#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* 甲言 完全中文化: 字符串库函数中文映射测试 (2026-08-27):
 *   查找子串→strstr, 拼接→strcat, 转大写→toupper, 转小写→tolower,
 *   判数字→isdigit, 判字母→isalpha, 取字长→strlen, 倒序→strrev
 */

int main(void) {
    /* ① 查找子串 */
    const char *s = "hello world";
    char *p = 查找子串(s, "world");   /* strstr */
    if (!p || p - s != 6) return 1;

    /* ② 拼接 */
    char buf[32] = "foo";
    拼接(buf, "bar");                  /* strcat(buf, "bar") */
    if (strcmp(buf, "foobar")) return 2;

    /* ③ 字符分类 */
    if (!判数字('7')) return 3;        /* isdigit('7') */
    if (判数字('x')) return 4;
    if (!判字母('a')) return 5;        /* isalpha('a') */

    /* ④ 转大写/小写 (返回 int) */
    int up = 转大写('a');              /* toupper('a') = 'A' */
    int lo = 转小写('Z');              /* tolower('Z') = 'z' */
    if (up != 'A' || lo != 'z') return 6;

    /* ⑤ 取字长 */
    int n = 取字长("abcde");           /* strlen("abcde") = 5 */
    if (n != 5) return 7;

    /* ⑥ 与已有特性组合 */
    夹取 n, 0, 3;
    if (n != 3) return 8;

    printf("PASS\n");
    return 0;
}
