// @EXPECTED exit:0
#include <stdio.h>
/* S2a (2026-09-09): 可空 声明 无初始化器 → 确定性空 (注入 0) + 空值合法 (免 A4001) — 值类型/指针 双形态 */
int main(void) {
    可空 整 x;              /* 无初始化器 → 空 (0 哨兵) */
    若 (!判空 x) { printf("FAIL: 未初始化可空整 应判空=true\n"); return 1; }
    x = 5;
    若 (判空 x) { printf("FAIL: 赋值5后应非空\n"); return 2; }
    整 v = 解包 x;
    若 (v != 5) { printf("FAIL: 解包=%d 应5\n", v); return 3; }

    可空 整 *p;             /* 无初始化器 → 空 */
    若 (!判空 p) { printf("FAIL: 未初始化可空指针 应判空=true\n"); return 4; }
    整 实 = 7;
    p = &实;
    若 (判空 p) { printf("FAIL: 指针赋值后应非空\n"); return 5; }
    判空 p;                 /* 断言非空 (语句级窄化) */
    整 w = *p;
    若 (w != 7) { printf("FAIL: 解引用=%d 应7\n", w); return 6; }
    printf("PASS\n");
    return 0;
}
