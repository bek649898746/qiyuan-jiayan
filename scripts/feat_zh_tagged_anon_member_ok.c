// @EXPECTED out:C=16,0,8 D=24,0,8 cx=1 cp=42 dx=2 da=7
/*
  A56 (2026-09-14): **具标签 无变量名 的 结构体/联合 成员** (C11 6.7.2.1p13 匿名成员).

  病灶:  `struct C { int x; union CU { int p; double q; }; };` —— `union CU {...};` 有 tag 却无 变量名,
    在 C11 里 仍 是 **匿名成员** (其 成员 提升 到 外层 作用域)。甲言 修前:
      · 尺寸 完全 不 计入 (`sizeof(struct C)` = 4, gcc 16) —— 内联 定义 被 当 纯 类型 定义;
      · 成员 不 提升 (`c.p` 报 A9001「类型 'C' 无字段 'p'」)。

  修法 (A56):
    · **L16508 补 `否 若 (内构序 >= 0)` 分支** —— 原链 只 有「有 变量名」的 `若`, 无 变量名 时 整块 跳过
      (与 A53 的 无 tag 匿名 是 同一语义 两条 路径: A53 修 了 无 tag, 有 tag 走 本条 内联定义 路径)。
    · **类型 标记位 整体 搬运** —— 原 只 传 6 个 参数 (正辰/现捌/域行数/毛利/正聚) →
      副优(双精度)/原进(指针)/内联/符肆/主挂/高贰/副戌/值劣/间次一/高玖/字段函浮 全丢。

  覆盖:  外层 struct + 内 union / 外层 struct + 内 struct; sizeof + offsetof + 提升后 字段 读写。
  神谕:  同名 程序 `gcc -w` 编译, 输出 逐字节 全等。

  ⚠️ 未修 (挂账, 本锚 不 覆盖):
    · **外层 是 union 的 形态** (`union E { int x; union EU {...}; };` — gcc 8, 甲言 16):
      已 加 `类型表[构序].是否联` 分支 但 实测 未命中 ⟹ E 走 的 是 **另一条 路径** (待 定位)。
    · **双精度 字段 在 内联定义 里 读 0.0** (`struct F { int x; struct FS { int a; double b; } fs; };`
      的 `f.fs.b`): 既存缺陷 —— 主路径 有 `末进一`/`目薄一` → `构域浮`, 内联定义 路径 没有;
      循环 A/B 已 补 `双进A`/`双进B` 但 实测 未覆盖 FS 这条 (同样 走 另一份 路径)。
*/
#include <stdio.h>
#include <stddef.h>

struct C { int x; union CU { int p; double q; }; };   /* 内联 union, 有 tag 无 变量名 */
struct D { int x; struct DS { int a; double b; }; };  /* 内联 struct, 有 tag 无 变量名 */

int main(void) {
    printf("C=%d,%d,%d D=%d,%d,%d",
           (int)sizeof(struct C), (int)offsetof(struct C, x), (int)offsetof(struct C, p),
           (int)sizeof(struct D), (int)offsetof(struct D, x), (int)offsetof(struct D, a));
    struct C c; c.x = 1; c.p = 42;
    struct D d; d.x = 2; d.a = 7;
    printf(" cx=%d cp=%d dx=%d da=%d\n", c.x, c.p, d.x, d.a);
    return 0;
}
