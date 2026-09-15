// @EXPECTED out:PASS
#include <stdio.h>

/* D1 (2026-09-11) 回归 锚: 动态数组 (Vec) 的 遍-范围 迭代
 *   ① 值 迭代 `遍 x 于 v`          — 原 预声明 x 误报 A4001 (同 D1 主修)
 *   ② 索引+值 `遍 i, x 于 v`       — 原 Vec 路径 **从未 给 索引变量 赋值** → i 恒 0 (静默)
 *      (数组/切片 路径 早已 有 `i = _i;`; Vec 路径 漏 挂 → 本 站 补 齐) */

int main(void) {
    动态数组 v;
    追加 v, 10;
    追加 v, 20;
    追加 v, 30;

    /* ① 值 迭代 */
    int x;
    int s = 0;
    遍 x 于 v { s += x; }
    printf("sum=%d\n", s);
    if (s != 60) return 1;

    /* ② 索引+值 迭代 */
    int i, y;
    int t = 0;
    遍 i, y 于 v { t += i * y; }
    printf("idx=%d\n", t);
    if (t != 0 * 10 + 1 * 20 + 2 * 30) return 2;

    /* ③ 追加 后 再 迭代: 长度 跟随 */
    追加 v, 40;
    int c = 0;
    遍 y 于 v { c++; }
    printf("n=%d\n", c);
    if (c != 4) return 3;

    printf("PASS\n");
    return 0;
}
