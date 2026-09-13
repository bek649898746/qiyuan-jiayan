// @EXPECTED out:u2=16,0,8 s1=24,8,16
/* A52 锚 (2026-09-14, 不动点 9F145946 → 待填): **union 的 内联匿名 结构体成员** 的 union 语义。
   病灶: `union u2 { int file; struct { int a; double b; } buf; }` —— 内联匿名 struct 成员的登记路径
     **无条件 走 `字段登记` (顺序累加)**, 漏了 union 分支 ⟹ union 被当 结构体 布局:
     尺寸 4+pad+16 = 24 (应 16), `buf` 偏移 8 (应 0)。
   **同形 具名 struct 成员 (`union u1 { int file; struct Inner buf; }`) 是 对的** —— 它走 L16446 的
     `若 (源高) 联合字段(...)`; **内联匿名 struct 走 另一处**, 而 那处 漏了 联合 分支。
   **插桩实证**: 加 `[A52-16446]` 探针后编译, 只有 `u1` 命中 (源高=1 构名=u1), `u2`/`s1` **不命中** ⟹ 一次定位。
   修法: 该处 补 `若 (源高) { 联合字段(...); 字段类型(构序, 源酉, 深进); } 否 字段登记(...)`
     (回填 `毛利` 类型序, 否则 嵌套访问 `buf.a` 找不到类型)。

   覆盖: u1 具名 struct 成员 (对照, 本就对) · u2 内联匿名 struct 成员 (union 语义 + 内部偏移) ·
         s1 struct + 内联匿名 struct 成员 (累加语义 + 偏移累加)。
   证据: 输出与 gcc **逐值全等**。 */
#include <stdio.h>
#include <stddef.h>

struct Inner { int a; double b; };
union u1 { int file; struct Inner buf; };
union u2 { int file; struct { int a; double b; } buf; };
struct s1 { int x; struct { int a; double b; } buf; };

int main(void) {
    printf("u1=%d,%d Inner=%d,%d\n",
        (int)sizeof(union u1), (int)offsetof(union u1, buf),
        (int)offsetof(struct Inner, a), (int)offsetof(struct Inner, b));
    printf("u2=%d,%d,%d s1=%d,%d,%d\n",
        (int)sizeof(union u2), (int)offsetof(union u2, buf), (int)offsetof(union u2, buf.b),
        (int)sizeof(struct s1), (int)offsetof(struct s1, buf), (int)offsetof(struct s1, buf.b));
    return 0;
}
