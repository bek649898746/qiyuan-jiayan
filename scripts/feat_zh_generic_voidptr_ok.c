// @EXPECTED out:PASS 空指针分支
#include <stdio.h>

/* A28 (2026-09-11, 随 A22 同 站 闭环) 回归 锚: `空 *` 关联 类型 必须 消费 `*`
 *   旧 行为: 中十() 的 空/void 分支 只 `词位++` 就 返 8, 漏 消费 其 后 的 `*` →
 *     该 分支 的 关联表达式 被 当 成 `* 值` (解引用) 解析, 且 冒号 检查 落空 →
 *     词序 错位, 后续 关联 分支 全 错; 现象 = _Generic 静默 返回 指针 值/0, 无 任何 诊断。
 *   本 锚: 空* 分支 分别 放 在 列表 首/中/尾, 并 与 同 列表 的 整数* / 双精度* / 整 对照。 */

int main(void) {
    int i = 0; char c = 0; double d = 0;
    int *pi = &i; char *pc = &c; double *pd = &d; void *pv = 0;

    /* 空* 在 首位 */
    int a = _Generic((pc), void *: 8, char *: 2, default: 99);
    int b = _Generic((pi), void *: 8, int *: 1, default: 99);
    /* 空* 在 中部 */
    int c1 = _Generic((pd), int *: 1, void *: 8, double *: 3, default: 99);
    int c2 = _Generic((pc), char *: 2, void *: 8, default: 99);
    /* 空* 在 末位 */
    int e1 = _Generic((pv), int *: 1, char *: 2, void *: 8, default: 99);
    /* 非 指针 关联 与 空* 混排 */
    int f1 = _Generic((i), int: 11, void *: 8, default: 99);
    int f2 = _Generic((d), void *: 8, double: 23, default: 99);

    if (a != 2) return 1;
    if (b != 1) return 2;
    if (c1 != 3) return 3;
    if (c2 != 2) return 4;
    if (e1 != 8) return 5;
    if (f1 != 11) return 6;
    if (f2 != 23) return 7;

    printf("PASS 空指针分支\n");
    return 0;
}
