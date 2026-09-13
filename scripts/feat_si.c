// @EXPECTED out:INT64_MAX=9223372036854775807 INT64_MIN=-9223372036854775808
/* A51 (2026-09-13): C99 7.18.2 整数 极值 宏 —— 原 **stdint.h 预置 只有 typedef, 完全 无极值宏** ⟹
   `INT64_MAX`/`INT64_MIN`/`UINT64_MAX` 等 报「未声明标识符」(本锚 原 甲言 **编译失败**);
   而 内建 `<限制.h>` 只 有 `LLONG_MAX` 系 的 **另一套名字** ⟹ 标准 C 代码 用 `INT64_MAX` 直接 死。
   修: 在 stdint.h 预置 补 INT8/16/32/64 + INTPTR + SIZE + PTRDIFF + INTMAX 系 极值宏 (负值 不括号, 同 限制.h 既有风格)。
   发现路径: **层3 A 类扫锚** (gcc vs 甲言 对账 42 个纯 C 锚 → 本锚 报「编译异常」)。
   证据: 输出 与 gcc 逐字节 一致。 */
#include <stdio.h>
#include <stdint.h>
int main(void) {
    long long a = INT64_MAX;
    long long b = INT64_MIN;
    printf("INT64_MAX=%lld INT64_MIN=%lld\n", a, b);
    return 0;
}
