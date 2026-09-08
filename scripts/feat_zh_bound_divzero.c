// @EXPECTED exit:nonzero
#include <stdio.h>
/* 显式F2 负例 (2026-09-07): 运行时除零 → __qcc_div_check 优雅 exit(1) */
/* B8 补 (2026-09-09): 赋值 RHS 除法 / if 块内除法 漏插检查 (原 SIGFPE 崩) —
   修复: 赋值() RHS 解析前清 语句表达式丢弃 (RHS 值真实使用非丢弃) */
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
