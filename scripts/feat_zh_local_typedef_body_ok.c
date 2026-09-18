// @EXPECTED out:LTD 3 4 7 8 68 5 6 11 33 1 2 13 14
/* 锚: 函数内 `typedef struct/union { ... } 别名;`（带体形态）— C104/C105/C106 (第 101 站, 2026-09-18)
   缺陷史: 局部 typedef 的兜底是「跳到首个分号」:
       循环 (词位 < 词容量 && 词型表[词位] != 分号 && 词型表[词位] != 词终) 词位++;
   对带体形态会停在**内层字段**的分号 (`int a;` 的那个) ⟹ 余下 `int b; } X;` 变垃圾
   ⟹ **其后所有语句被静默吞掉** (最小复现: printf 全丢、rc 仍 0; `X v; v.a` 读垃圾值)。
   既有注释自认「带体 保持整句拒绝=行为一致」，实为静默跳过且跳错位置。
   三处修法: C104 遇 `{` 先配对跳过; C105 带标签形态 建类型+体注册+登记别名;
             C106 匿名体形态 (类型起名 `__anon_<序>`)。
   锚灵敏度: 在 C103 版上 stdout **为空** (全部语句被吞) ⟹ 本锚 @EXPECTED out: 不符 ⟹ 必红。
   覆盖形态: 具名 struct / union / 匿名体 / 嵌套结构体字段 / 数组 / char+int / 多 typedef 并存。
   对照形态 (无体 `typedef int`、不用 typedef 的 `struct T {...};`) 本已正常, 一并钉死防回归。 */
#include <stdio.h>

typedef struct Outer { int x; } Outer;

int main(void) {
    int r = 0;

    /* ① 具名 struct ② 具名 union ③ 匿名体 ④ 两个 typedef 并存 */
    typedef struct T { int a; int b; } X;
    typedef union U { int i; char c[4]; } Y;
    typedef struct { int a; int b; } Z;
    typedef struct H { int p; } W;

    X v; v.a = 3; v.b = 4;
    Y u; u.i = 0x41424344;
    Z z; z.a = 7; z.b = 8;
    W w; w.p = 9;
    if (v.a != 3 || v.b != 4) r |= 1;
    if (u.c[0] != 0x44) r |= 2;
    if (z.a != 7 || z.b != 8) r |= 4;
    if (w.p != 9) r |= 8;

    /* ⑤ 嵌套结构体字段  ⑥ 数组  ⑦ char+int 混合 */
    typedef struct N { Outer o; int b; } NX;
    NX n; n.o.x = 5; n.b = 6;
    if (n.o.x != 5 || n.b != 6) r |= 16;

    typedef struct A { int a; } AX;
    AX arr[3]; arr[0].a = 11; arr[1].a = 22; arr[2].a = 33;
    if (arr[0].a != 11 || arr[1].a != 22 || arr[2].a != 33) r |= 32;

    typedef struct C { char c; int n; } CX;
    CX c; c.c = 1; c.n = 2;
    if (c.c != 1 || c.n != 2) r |= 64;

    /* ⑧ typedef 之后语句必须存活 */
    typedef struct P { int a; int b; } PX;
    PX p; p.a = 13; p.b = 14;
    if (p.a != 13 || p.b != 14) r |= 128;

    /* 对照形态: 无体 typedef / 不用 typedef 的体定义 */
    typedef int MYINT;
    MYINT m = 15;
    if (m != 15) r |= 256;

    printf("LTD %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
           v.a, v.b, z.a, z.b, (int)u.c[0], n.o.x, n.b, arr[0].a, arr[2].a,
           (int)c.c, c.n, p.a, p.b);
    return r;
}
