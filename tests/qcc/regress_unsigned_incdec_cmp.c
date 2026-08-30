// @EXPECTED out:ok=1
// 2026-08-31 csmith s11: int32<uint32 比较 — 前自减节点 (case 23) 结果类型=操作数类型
// (--(*uint32_ptr)) 原 expr_is_unsigned 不识别 → 按有符号比较 → 分支翻转
#include <stdio.h>
#include <stdint.h>
int32_t g_577 = 5047;
uint32_t g_21 = 0;
uint32_t gv = 0xFFFFFFFF;
int main(void) {
    uint32_t *l_20 = &g_21;
    int b = g_577 < (--(*l_20));           /* 5047 < 0xFFFFFFFF → 1 (unsigned) */
    g_21 = 0;
    int c = g_577 < (--g_21);              /* 同上 (局部自减) */
    int d = 5047 < (uint32_t)0xFFFFFFFFu;  /* 控制: 常量比较 */
    int ok = (b == 1) && (c == 1) && (d == 1);
    printf("ok=%d\n", ok);
    return 0;
}
