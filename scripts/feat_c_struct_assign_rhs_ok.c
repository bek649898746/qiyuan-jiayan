// @EXPECTED out:A=3x B=1y C=2z D=2z E=3x S=123
/* A71-续5 (2026-09-15): 构体整体赋值 —— **右操作数家族** 全形态。
   病灶: 构体整块拷贝的判据链只认 RHS ∈ {变量 / 解引用 / 调用 / 赋值 / 三目},
         **不认 下标 a[i] / 成员 s.f / 箭头 p->f** ⟹ `t = a[i]` 落 标量兜底分支,
         只拷首 4 字节 (实测 完全没拷: t.k=0); 反向 `a[i] = t` 正常, 声明初始化 `T t = a[i]` 也正常。
   根治: 新增一条与 变量/解引用 分支平级的 RHS ∈ {下标, 箭头, 成员} 分支 —
         复用「目标地址压栈 → 免解引用取源地址 → 深利(尺寸)」序列 (镜像 `b = *p`)。
   本锚钉: 五种源形态 (变量 / 数组元素 / 结构体成员 / 箭头字段 / 解引用) + 结构体数组选择排序,
          全部与 gcc 逐字节一致。 */
#include <stdio.h>
#include <string.h>

struct P { int k; char n[8]; };
struct Outer { struct P in; };

int main(void) {
    struct P a[3], t, s;
    struct P *p = &a[2];
    struct Outer o;
    int i, j;

    a[0].k = 3; strcpy(a[0].n, "x");
    a[1].k = 1; strcpy(a[1].n, "y");
    a[2].k = 2; strcpy(a[2].n, "z");

    s = a[0];
    printf("A=%d%s ", s.k, s.n);          /* 变量源 */

    t = a[1];
    printf("B=%d%s ", t.k, t.n);          /* 数组元素 (静态下标) */

    i = 2;
    t = a[i];
    printf("C=%d%s ", t.k, t.n);          /* 数组元素 (变量下标) */

    t = *p;
    printf("D=%d%s ", t.k, t.n);          /* 解引用源 */

    o.in = a[0];
    t = o.in;
    printf("E=%d%s ", t.k, t.n);          /* 成员 / 箭头 家族 */

    for (i = 0; i < 3; i++)
        for (j = i + 1; j < 3; j++)
            if (a[i].k > a[j].k) { t = a[i]; a[i] = a[j]; a[j] = t; }
    printf("S=%d%d%d\n", a[0].k, a[1].k, a[2].k);
    return 0;
}
/* @EXPECTED out:S=123 */
