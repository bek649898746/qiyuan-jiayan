// @EXPECTED out:anonmem A=16,8 B=16,8 D=24,16
/* A53 锚 (2026-09-13, 不动点 4E2FE8BF → 待填): **C11 匿名 结构体/联合 成员** (无 变量名)。
   病灶: 字段 位置 的 `结构词 + 左花` 解析 出 匿名类型后, **只有 「有 变量名」(`union { ... } u;`) 才 把 它 登记 到 外层**;
     **无 变量名** (`union { ... };` / `struct { ... };`) 时 **匿名类型 成 孤儿** ⟹
     外层 既 无 该 字段 (`offsetof` 返 -1) 也 不计 尺寸 (整体 尺寸 少 一块)。
   修法: **扁平化** (C11 6.7.2.1: 匿名 成员 的 名字 提升 到 外层 作用域) ——
     联合 成员 偏移 都 = 基偏; 结构体 成员 = 基偏 + 内部偏移; 外层 尺寸 = 基偏 + 匿名体尺寸 (非 成员 累加); 对齐 取 max。

   覆盖 (gcc 逐值 对照): A 匿名 union 成员 · B 具名 变量 (回归 保护, 本 就 正确) · C 具标签 无变量 · D 匿名 struct 成员.
   ⚠️ C 形态 (`union U { ... };` **有 标签** 无 变量): gcc 亦 输出 16 (成员 提升), 甲言 仍 4 ——
     走 **另 一条 路径** (L16333「内联 定义 函数体」), **未 在 本 站 修**, 见 `_DEBT_LEDGER` A53 附注。 */
#include <stdio.h>
#include <stddef.h>

struct A { int x; union { int p; double q; }; };
struct B { int x; union { int p; double q; } u; };
struct C { int x; union CU { int p; double q; }; };
struct D { int x; struct { int p; double q; }; };

int main(void) {
    printf("anonmem A=%d,%d B=%d,%d D=%d,%d C=%d\n",
        (int)sizeof(struct A), (int)offsetof(struct A, q),
        (int)sizeof(struct B), (int)offsetof(struct B, u.q),
        (int)sizeof(struct D), (int)offsetof(struct D, q),
        (int)sizeof(struct C));
    return 0;
}
