/* feat_c_limits_min_ok.c — 内建 `<limits.h>` 的 INT_MIN / LONG_MIN 取值正确 (A107 根治 2026-09-14)
   病灶: 内建 limits.h 里 `#define INT_MIN -2147483647` —— **直接少一** (应 `(-2147483647 - 1)`);
         且 即使 写全, 字面量 绝对值 `2147483648` 也 超 int 正上限 ⇒ 常量解析 会 饱和/溢出.
         实测 `printf("%d\n", INT_MIN)` 与 gcc 差 1 (`-2147483647` vs `-2147483648`); `%ld` 同病.
   修法: 三个 最小值宏 改 **表达式形式** `(-MAX - 1)`, 与 C 标准头 **逐字一致**:
         `INT_MIN (-2147483647 - 1)` / `LONG_MIN (-2147483647L - 1L)` / `LLONG_MIN (-9223372036854775807LL - 1LL)`.
   本锚: INT_MIN / INT_MAX / LONG_MIN 打印. */
// @EXPECTED out:-2147483648
// @EXPECTED out:2147483647
// @EXPECTED out:-2147483648
// @EXPECTED exit:0
#include <stdio.h>
#include <limits.h>

int main(void)
{
    long w = LONG_MIN;

    printf("%d\n", INT_MIN);
    printf("%d\n", INT_MAX);
    printf("%ld\n", w);
    return 0;
}
