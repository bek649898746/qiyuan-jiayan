#include <stdio.h>

/* 甲言 error union 接入测试 (2026-08-28): 机制层显式族 P0 #72 — 错误 T f = T f (错误标记吞掉);
   错 N; → return N; 显式错误码返回 (Go 式 error union, AI 找根因: 错误显式 + 调用方检查) */

错误 int f(int x) {   /* error union 函数: 返回 int 或错误码 */
    if (x < 0) 错 100;  /* 显式错误码返回 */
    return x * 2;
}

int main(void) {
    /* ① 错误路径: 错 100 → return 100 */
    int v = f(-5);
    if (v != 100) { printf("FAIL err: %d\n", v); return 1; }

    /* ② 成功路径: 正常返回 */
    int w = f(7);
    if (w != 14) { printf("FAIL ok: %d\n", w); return 1; }

    /* ③ 调用方显式检查错误码 (错误显式处理) */
    int r = f(-1);
    if (r == 100) { /* 识别错误码并处理 */ }
    else { printf("FAIL check: %d\n", r); return 1; }

    printf("PASS\n");
    return 0;
}
