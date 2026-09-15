/* A71-续3 (2026-09-15): 调用基 / 赋值基 成员访问 —— **正向** 合法路径不误伤 + 取值正确。
   病灶 (A71-续2 同站发现, 当时记挂账): 赋值表达式作构值成员基时 codegen 无「构值赋值」通道 ⟹
     `(s = t).a` 值错 (读 垃圾 / t.b) 且 赋值副作用 **从未发射** (`int x = (s = t).a;` → x=90, s.a=0);
   根治: ① `内存地址()` 增 节点_赋值 分支 — C 语义: (s = t) 是「修改后 s」的左值 ⟹
           先 发射赋值副作用, 再 取 LHS 地址;
         ② 成员读 codegen 的「链基」判据 补 10 (赋值) — 原 判据 只列 12/13/15/4/22 ⟹ 赋值基
           根本不放行到 `内存地址`, 分支加了也走不到。
   本锚钉: 构值返回调用 (f().字段) / 指针返回调用 (f()->字段) / 赋值表达式 ((s=t).字段,
          含「赋值确实写入 LHS」 — G) 三路径 取值与 gcc 逐字节一致。 */
#include <stdio.h>

struct S { int a; long b; };

struct S mk(int v) { struct S r; r.a = v; r.b = v * 10; return r; }
struct S *mkp(void) { static struct S s = { 7, 70 }; return &s; }

int main(void) {
    struct S s, t;
    s.a = 0; s.b = 0; t.a = 9; t.b = 90;
    printf("A=%d B=%ld ", mk(3).a, mk(3).b);
    printf("C=%d D=%ld ", mkp()->a, mkp()->b);
    printf("E=%d F=%ld ", (s = t).a, (s = t).b);
    printf("G=%d\n", s.a);
    return 0;
}
/* @EXPECTED out:A=3 B=30 C=7 D=70 E=9 F=90 G=9 */
