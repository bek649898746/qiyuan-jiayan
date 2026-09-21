// @EXPECTED out:PASS
#include <stdio.h>

/* R241 补锚 — **嵌套内层字段的初始化追踪**（A4001 误报根治）
 *
 * 病灶（R234 二分 + R241 读码确认）：写**嵌套内层字段**（`o.leaf.a = 2`）时，
 *   赋值路径用的单级 `尾写字段` 拿到字段名 `a` **不在宿主 `struct O` 里** ⟹ `字段序 < 0`
 *   ⟹ 原实现**清空整个就绪位图（降级）** ⟹ 之后读**该对象任意字段**（如 `o.x.a`）都走
 *   `尾已写`（`位序<0 返 0`）⟹ **误报 A4001**（拒绝合法 C）。
 *   触发需 `结构体指针字段 + 结构体数组字段 + 写结构体值字段内层` 三者同时在场（去掉任一即绿）。
 * R241 修法：该分支改为**保守整体初始化**（`未初始化移除(值)`），与同函数上方
 *   「联合成员写 = 整对象初始化」(H1o) 同一保守原则 ⟹ 误报 → 准许。
 *
 * ⚠️ 本锚刻意**不用** `= { 0 }` 初始化、**不用类型词作标识符** —— 正是原红例形态。
 */

struct Leaf { int a; char b; };
struct O {
    int n;
    struct Leaf leaf;
    struct Leaf *pl;
    struct Leaf arr[2];
    struct Leaf x, y;
};

int main(void) {
    struct O o;                       /* ← 裸声明：正是触发 A4001 的形态 */

    o.n = 1;
    o.leaf.a = 2;  o.leaf.b = 3;      /* 写「结构体值字段」的内层 ⟹ 旧实现此处降级 */
    o.pl = &o.leaf;
    o.arr[0].a = 4;
    o.x.a = 8;  o.y.a = 9;

    if (o.n != 1) return 1;
    if (o.leaf.a != 2 || o.leaf.b != 3) return 2;
    if (o.pl == 0 || o.pl->a != 2) return 3;
    if (o.arr[0].a != 4) return 4;
    if (o.x.a != 8 || o.y.a != 9) return 5;   /* ← 旧实现此处误报 A4001 */

    printf("PASS\n");
    return 0;
}
