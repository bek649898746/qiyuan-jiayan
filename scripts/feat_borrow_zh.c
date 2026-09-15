// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 简化借用 (借用 标记) 接入测试 (2026-08-27) — 2026-08-28 完善更新:
 *   第 94 站起 借用 从"词法层吞掉"改为真关键字 (声明形 `借用 const int *p = &x;` 作废)
 *   语句形: 借用 x; 标记只读 (读 OK, 写=自动放还, 见 feat_borrow_auto_zh.c); 放还 x; 解除
 *   本测试更新为语句形 + 指针读组合 (原声明形语义, Rust 风格借用契约)
 */

int main(void) {
    /* ① 只读借用 + 指针读 */
    int x = 42;
    借用 x;
    const int *p = &x;   /* 借用期取地址读 OK */
    if (*p != 42) return 1;
    放还 x;

    /* ② 可变借用 + 放还后可写 */
    int y = 10;
    可变借用 y;
    放还 y;
    y = 20;
    if (y != 20) return 2;

    /* ③ 借用 + 指针运算 (数组) */
    int arr[3] = { 1, 2, 3 };
    借用 arr;
    const int *a = arr;
    if (*(a + 1) != 2) return 3;
    放还 arr;

    /* ④ 与已有特性组合 */
    夹取 y, 0, 15;             /* 20 → 15 */
    范围 y 于 0 到 15;
    if (y != 15) return 4;

    printf("PASS\n");
    return 0;
}
