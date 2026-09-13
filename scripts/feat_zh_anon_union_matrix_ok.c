// @EXPECTED out:S1=24 S2=16 U1=16 U2=8 N1=24 N2=16 up=0 ud=4 sp=0
/*
  A64 (2026-09-14): 「具标签 无变量名」匿名成员 的 **四矩阵** —— 外层 struct/union × 内层 struct/union.

  演进: A53 修「无 tag」; A56 修「有 tag + 外层 struct」; **A64 修「外层 union + 内层 union」(U2)**。
  四矩阵 实验 (scratch/_a57_matrix.c) 精确定位 —— **只有 U2 错**:

     形态                              gcc   甲言(A64 前)
     S1 外 struct + 内 struct          24    24  ✓
     S2 外 struct + 内 union           16    16  ✓
     U1 外 union  + 内 struct          16    16  ✓
     **U2 外 union + 内 union            8    16**  ✗   ← 本锚 钉的 就是 这个

  **根因 (探针 铁证)**: 外层 union 的 尺寸 应 **取 max**; 但 提升 时 `字段登记(构序, ...)` 会把 **每个 提升 字段 累加进 外层 尺寸**
    (U2: `int x` → 4, 提升 `p` → +4 = 8, `q` → +8 = **16**), 而 原代码 的 max 分支拿 **当前(已被污染的)尺寸** 去比
    (16 > 8) ⟹ **max 永远 失效**, 尺寸 停在 16。
  **修法**: 在 `字段登记` 循环 **之前** 存 `整 旧尺寸 = 类型表[构序].尺寸;`（= 基偏 计算所用的那个值），
    max 时 与 **旧尺寸** 比: `类型表[构序].尺寸 = (新高 > 旧尺寸) ? 新高 : 旧尺寸;`
  ⚠️ 两处 兄弟分支 同修: A56 路径 (`内构序`) 与 A53 路径 (`深进`)。
  ⚠️ 外层 union 判定 必须用 `类型表[构序].是否联` —— 路径里的 `源高`/`高四一` 是 **内层** 类型标志。

  覆盖: 四矩阵 sizeof + **具名成员对照组** (N1/N2, 防改坏) + 提升后 字段 读写
    (union 覆盖语义: `u.p` 被 `u.q` 覆盖 → 读 0; `ud` = 3.5 → `%.0f` 得 4; `sp` 同理 0)。
  神谕: 同名 程序 `gcc -w` 编译, 输出 逐字节 全等。
*/
#include <stdio.h>

struct S1 { int x; struct  A { int p; double q; }; };   /* 外 struct + 内 struct */
struct S2 { int x; union   B { int p; double q; }; };   /* 外 struct + 内 union  */
union  U1 { int x; struct  C { int p; double q; }; };   /* 外 union  + 内 struct */
union  U2 { int x; union   D { int p; double q; }; };   /* 外 union  + 内 union  */

struct N1 { int x; struct NA { int p; double q; } n; }; /* 具名成员 对照 (应全对) */
struct N2 { int x; union  NB { int p; double q; } n; };

int main(void) {
    printf("S1=%d S2=%d U1=%d U2=%d N1=%d N2=%d",
           (int)sizeof(struct S1), (int)sizeof(struct S2),
           (int)sizeof(union U1), (int)sizeof(union U2),
           (int)sizeof(struct N1), (int)sizeof(struct N2));
    union U2 u; u.x = 5; u.p = 7; u.q = 3.5;   /* 提升后 直接访问 D 的成员 */
    struct S2 s; s.x = 1; s.p = 2; s.q = 2.5;
    printf(" up=%d ud=%.0f sp=%d\n", u.p, u.q, s.p);
    return 0;
}
