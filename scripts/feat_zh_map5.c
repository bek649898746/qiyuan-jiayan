// @EXPECTED out:PASS
#include <stdio.h>
#include <string.h>

/* 甲言 中文化映射 第5批 测试 (2026-08-28): 格式化到串/内存比较/移动内存 */

int main(void) {
    char buf[64];
    格式化到串(buf, "v=%d", 42);
    if (字比(buf, "v=42") != 0) { printf("FAIL sprintf: %s\n", buf); return 1; }
    if (内存比较("abc", "abd", 2) != 0) { printf("FAIL memcmp\n"); return 1; }
    char m1[8] = "abcdef";
    char m2[8] = {0};
    移动内存(m2, m1, 6);
    if (字比(m2, "abcdef") != 0) { printf("FAIL memmove: %s\n", m2); return 1; }
    printf("PASS\n");
    return 0;
}
