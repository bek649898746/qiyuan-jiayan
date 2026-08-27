#include <stdio.h>
#include <string.h>

/* 甲言 原串 接入测试 (2026-08-28): 原始/多行字符串 — 原串"..." / 原串#"..."# (N 个 # 定界), 不转义, 可跨行 */

int main(void) {
    /* 基本 */
    if (strcmp(原串"hello", "hello") != 0) { printf("FAIL raw1\n"); return 1; }
    /* 不处理转义: \n 保持字面 (源码 4 字符: \ n 两字符即 '\\','n') */
    if (strcmp(原串"a\nb", "a\\nb") != 0) { printf("FAIL raw2: %s\n", 原串"a\nb"); return 1; }
    /* 多行 (源码内真实换行) */
    if (strcmp(原串"line1
line2", "line1\nline2") != 0) { printf("FAIL raw3\n"); return 1; }
    /* # 定界: 内部可含单个 " */
    if (strcmp(原串#"say "hi""#, "say \"hi\"") != 0) { printf("FAIL raw4: %s\n", 原串#"say "hi""#); return 1; }
    /* ## 定界: 内部可含 "# */
    if (strcmp(原串##"a"#b"##, "a\"#b") != 0) { printf("FAIL raw5: %s\n", 原串##"a"#b"##); return 1; }
    /* 转义符保持字面 */
    if (strcmp(原串"tab\there", "tab\\there") != 0) { printf("FAIL raw6: %s\n", 原串"tab\there"); return 1; }
    /* interning: 与普通字符串同内容复用 */
    char *p1 = 原串"abc";
    char *p2 = "abc";
    if (p1 != p2) { printf("FAIL raw intern\n"); return 1; }
    printf("PASS\n");
    return 0;
}
