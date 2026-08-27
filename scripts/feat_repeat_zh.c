#include <stdio.h>

/* 甲言 重复 接入测试 (2026-08-28): 重复 N { body } — 循环糖 */

int main(void) {
    int sum = 0;
    重复 5 {
        sum += 2;
    }
    if (sum != 10) { printf("FAIL repeat: %d\n", sum); return 1; }
    /* 嵌套 */
    int cnt = 0;
    重复 3 {
        重复 2 {
            cnt++;
        }
    }
    if (cnt != 6) { printf("FAIL nest: %d\n", cnt); return 1; }
    /* 变量次数 */
    int n = 4;
    int s2 = 0;
    重复 n {
        s2++;
    }
    if (s2 != 4) { printf("FAIL var: %d\n", s2); return 1; }
    printf("PASS\n");
    return 0;
}
