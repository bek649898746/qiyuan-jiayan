// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 确定性异常 (抛出/若错/捕获) 接入测试 (2026-08-27; 2026-10-18 #148 后改走运行时内置)。
 *   抛出 码     →  throw_ex(1, 码, "")，运行时置 ex_pending/cur_ex_code
 *   若错 语句   →  if (ex_pending) 语句;
 *   捕获;       →  if (ex_pending) { ex_pending = 0; }
 *   throw_ex / ex_pending / cur_ex_code 由 qcc_rt.c 提供 —— 本锚不再自带定义（否则 A6109）。 */

static int divide(int a, int b) {
    if (b == 0) { 抛出 7; return 0; }
    return a / b;
}

static int compute(int a, int b) {
    int r = divide(a, b);
    若错 return 0;
    return r + 1;
}

static int deep_compute(int a, int b) {
    int r = compute(a, b);
    若错 return 0;
    return r * 2;
}

int main(void) {
    /* ① 正常调用 (无异常) */
    ex_pending = 0;
    int r = deep_compute(10, 2);   /* ((10/2)+1)*2 = 12 */
    捕获;                          /* 不触发 */
    if (r != 12) return 1;
    if (ex_pending != 0) return 2;

    /* ② 深层除零 → 跨两层回卷 → 捕获吞掉 */
    ex_pending = 0;
    r = deep_compute(10, 0);
    捕获;
    if (r != 0) return 3;
    if (ex_pending != 0) return 4;
    if (cur_ex_code != 7) return 5;  /* 异常码保留在 cur_ex_code */

    /* ③ 多次抛出捕获 */
    ex_pending = 0;
    r = deep_compute(5, 0);
    捕获;
    if (r != 0 || ex_pending != 0) return 6;

    printf("PASS\n");
    return 0;
}
