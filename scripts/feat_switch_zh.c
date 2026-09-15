// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 依据 接入测试 (2026-08-28): switch 表达式 (有值) — 依据 r, x 对照 1: a, 2: b, 其他: c; */

int main(void) {
    /* 基本等值分发 + 缺省 */
    int r = 0;
    依据 r, 1 对照 1: 10, 2: 20, 3: 30, 其他: -1;
    if (r != 10) { printf("FAIL sw1: %d\n", r); return 1; }
    依据 r, 2 对照 1: 10, 2: 20, 3: 30, 其他: -1;
    if (r != 20) { printf("FAIL sw2: %d\n", r); return 1; }
    依据 r, 3 对照 1: 10, 2: 20, 3: 30, 其他: -1;
    if (r != 30) { printf("FAIL sw3: %d\n", r); return 1; }
    依据 r, 9 对照 1: 10, 2: 20, 3: 30, 其他: -1;
    if (r != -1) { printf("FAIL swdef: %d\n", r); return 1; }
    /* 无缺省: 未命中保持 0 */
    依据 r, 7 对照 1: 100, 2: 200;
    if (r != 0) { printf("FAIL swdef0: %d\n", r); return 1; }
    /* 变量选择表达式 */
    int x = 2;
    依据 r, x 对照 1: 111, 2: 222, 其他: 999;
    if (r != 222) { printf("FAIL swvar: %d\n", r); return 1; }
    /* 选择表达式为算式 */
    int a = 1, b = 2;
    依据 r, a + b 对照 3: 77, 其他: 0;
    if (r != 77) { printf("FAIL swarith: %d\n", r); return 1; }
    /* 结果为算式 */
    依据 r, 2 对照 1: a + 10, 2: b * 10;
    if (r != 20) { printf("FAIL swresexpr: %d\n", r); return 1; }
    /* 负值 case */
    依据 r, -1 对照 -1: 42, 其他: 0;
    if (r != 42) { printf("FAIL swneg: %d\n", r); return 1; }
    printf("PASS\n");
    return 0;
}
