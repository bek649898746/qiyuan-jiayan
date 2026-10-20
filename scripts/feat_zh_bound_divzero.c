// @EXPECTED exit:nonzero
// @EXPECTED err:[除零] 运行时除零错误
#include <stdio.h>
/* 显式F2 负例 (2026-09-07): 运行时除零 → __qcc_div_check 优雅 exit(1) */
/* B8 补 (2026-09-09): 赋值 RHS 除法 / if 块内除法 漏插检查 (原 SIGFPE 崩) —
   修复: 赋值() RHS 解析前清 语句表达式丢弃 (RHS 值真实使用非丢弃) */
/* F3 补 (2026-09-14): **闸盲区修补** —— 原锚 只 断言 `exit:nonzero`, 而 硬件崩 0xC0000094 **也 满足 nonzero**
   ⟹ "guard 缺失 → 硬崩" 在 闸里 完全 不可见 (F2 站后 曾 因此 误判 "闸全绿 ⟹ guard 正常");
   补 `out:[除零]` **消息级 断言** 后, "优雅退出" 与 "硬崩" 才 真正 可区分。
   ⚠️ 用 `out:` 而非 `err:` —— 甲言 产物 的 `fprintf(stderr, ...)` 实测 落到 **stdout**。 */
volatile int zero_g = 0;
int main(void) {
    int a = 10, b = 0;
    int c = a / b;
    printf("FAIL1: %d\n", c);
    return 0;
}
int extra(void) {
    int a = 10;
    int d = zero_g;              /* 运行时除数 */
    int r = 0;
    if (zero_g == 0) {
        r = a / d;               /* B8: if 块内赋值除法 曾漏插检查 SIGFPE */
        printf("FAIL2: %d\n", r);
    }
    return r;
}
