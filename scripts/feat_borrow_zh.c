#include <stdio.h>

/* 甲言 简化借用 (借用 标记) 接入测试 (2026-08-27):
 *   借用 const T *p = &x;  →  const T *p = &x (只读借用标记, 吞掉)
 *   借用 T *p = &x;        →  T *p = &x (可变借用标记, 吞掉)
 *   Rust 风格借用契约标记, 借用检查留未来诊断
 */

int main(void) {
    /* ① 只读借用 */
    int x = 42;
    借用 const int *p = &x;   /* const int *p = &x */
    if (*p != 42) return 1;

    /* ② 可变借用 */
    int y = 10;
    借用 int *q = &y;          /* int *q = &y */
    *q = 20;
    if (y != 20) return 2;

    /* ③ 借用 + 指针运算 */
    int arr[3] = { 1, 2, 3 };
    借用 const int *a = arr;   /* const int *a = arr */
    if (*(a + 1) != 2) return 3;

    /* ④ 与已有特性组合 */
    夹取 y, 0, 15;             /* 20 → 15 */
    范围 y 于 0 到 15;
    if (y != 15) return 4;

    printf("PASS\n");
    return 0;
}
