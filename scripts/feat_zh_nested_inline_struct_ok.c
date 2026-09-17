/* 锚: 嵌套内联结构体 的 字段类型序 (毛利) 登记 — 「R2 嵌套体」家族
   四个形态各锁一处解析块:
     ① `typedef union N1 { struct NK1 { V key_val; ... } u; } N1;`  → 内联具名体 块 (δ/17544)
     ② `typedef union N2 { struct { V key_val; ... } u; } N2;`        → 内联匿名体 块 (γ/17508)
     ③ `struct N3 { struct NK3 { V key_val; ... } u; };` (非 typedef) → 内联具名体 块 (16798)
     ④ ①+ 指针链 (p->u.key_val.i)                                    → 同上 但 走 箭头链
   判据: 全部 形态 编译通过 + 值 逐值 与 gcc 相同 (走 scripts/oracle 孪生 同源)。
   缺陷史: 字段只被 字段尺寸递归 建出、`字段类型`(唯一写 毛利 者) 从未调用
           ⟹ `A9001 类型 'NK' 的字段 'key_val' 不是结构体/联合`。 */
#include <stdio.h>
typedef union V { int i; } V;

typedef union N1 { struct NK1 { V key_val; int n; } u; int j; } N1;
typedef union N2 { struct { V key_val; int n; } u; int j; } N2;
struct N3 { struct NK3 { V key_val; int n; } u; int j; };
typedef union N4 { struct NK4 { V key_val; int n; } u; int j; } N4;

int main(void) {
    N1 a; N2 b; struct N3 c; N4 d; N4 *p = &d;

    a.u.key_val.i = 7;  a.u.n = 1;
    b.u.key_val.i = 11; b.u.n = 2;
    c.u.key_val.i = 13; c.u.n = 3;
    p->u.key_val.i = 17; p->u.n = 4;

    if (a.u.key_val.i != 7  || a.u.n != 1) return 1;
    if (b.u.key_val.i != 11 || b.u.n != 2) return 2;
    if (c.u.key_val.i != 13 || c.u.n != 3) return 3;
    if (p->u.key_val.i != 17 || p->u.n != 4) return 4;
    if (sizeof(N1) != sizeof(N2) || sizeof(N1) != sizeof(N4)) return 5;
    if (sizeof(N1) != 8) return 6;              /* union{ NK(8), int(4) } = 8 */
    if (sizeof(struct N3) != 12) return 7;      /* struct{ NK(8), int(4) } = 12 */
    printf("PASS\n");
    return 0;
}
