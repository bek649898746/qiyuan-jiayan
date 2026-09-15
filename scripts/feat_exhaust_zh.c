// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 模式匹配穷尽 接入测试 (2026-08-28): 机制层 #141 — 依据 r, x 穷尽 Shape 对照 ... 覆盖检查 */

int main(void) {
    判别 Color 于 { 红, 绿, 蓝 };
    struct Color c;
    c.tag = Color_绿;
    /* 穷尽覆盖: 红/绿/蓝 全匹配 → PASS */
    int r = 0;
    依据 r, c.tag 穷尽 Color 对照 Color_红: 10, Color_绿: 20, Color_蓝: 30;
    if (r != 20) { printf("FAIL exh1: %d\n", r); return 1; }
    /* 换变体 */
    c.tag = Color_蓝;
    依据 r, c.tag 穷尽 Color 对照 Color_红: 10, Color_绿: 20, Color_蓝: 30;
    if (r != 30) { printf("FAIL exh2: %d\n", r); return 1; }
    /* 无负载判别 + 穷尽 */
    判别 Status 于 { 成功, 失败 };
    struct Status s;
    s.tag = Status_成功;
    依据 r, s.tag 穷尽 Status 对照 Status_成功: 1, Status_失败: 0;
    if (r != 1) { printf("FAIL exh3: %d\n", r); return 1; }
    /* 常量正确性 */
    if (Color_红 != 0 || Color_绿 != 1 || Color_蓝 != 2 || Status_成功 != 0 || Status_失败 != 1) { printf("FAIL consts\n"); return 1; }
    printf("PASS\n");
    return 0;
}
