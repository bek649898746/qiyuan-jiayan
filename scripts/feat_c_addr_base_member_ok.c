// @EXPECTED out:A=1 B=2 C=1 D=2 E=10 F=20 G=1 H=2 I=5
/* A71-续 (2026-09-15): 取址基 / 转型基 成员访问 —— **正向** 合法路径不误伤 + 取值正确。
   病灶: 成员静态检查 (后缀链) 原 只 认 「变量 基」(节指元==0 && 节点_变量),
         取址基 (&s)->f、解引用基 (*p).f、转型基 ((结构体 S*)&s)->f 全 漏检 (实测 6/8 静默);
   根治: 序七() 增 分支 (节点_取址 下钻 / 节转序 转型目标构序) + 后缀链 检查改用 序七()。
   本锚钉: 四条 合法访问路径 取值 与 gcc 逐字节一致 (防 扩大检查 后 误伤)。 */
#include <stdio.h>

struct S { int a; long b; };
struct S g = { 5, 50 };

int main(void) {
    struct S s;
    struct S *p = &s;
    struct S arr[2];
    s.a = 1; s.b = 2;
    arr[0].a = 10; arr[0].b = 20;
    printf("A=%d B=%ld ", ((struct S*)&s)->a, ((struct S*)&s)->b);
    printf("C=%d D=%ld ", (&s)->a, (&s)->b);
    printf("E=%d F=%ld ", ((struct S*)&arr[0])->a, ((struct S*)&arr[0])->b);
    printf("G=%d H=%ld ", (*p).a, (*p).b);
    printf("I=%d\n", g.a);
    return 0;
}
/* @EXPECTED out:A=1 B=2 C=1 D=2 E=10 F=20 G=1 H=2 I=5 */
