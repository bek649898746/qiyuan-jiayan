/* feat_c_extern_inline_struct_ok.c — `extern 结构体 X { ... } 变量;` 内联定义的字段登记 (P0-21 根治 2026-09-14)
   病灶: `extern` 分支里 `结构体 X { ... }` 的**独立字段循环**相对主路径**整体不完整**:
         ① 只调 `字段尺寸递归` —— 漏 `原进`(指针标记) / `符肆` / `高贰` / **`毛利`(字段类型序)**;
         ② `结构体标签` **从未 `查类型`** ⟹ `域尺寸` 只特判 `char`/`double`;
         ③ **`结构体 X *a` 的 `*` 掉进末尾「兜底推进」被丢掉** —— 前置的 `循环 (乘)` 在
            「结构体词分支」**之前**, 而 `*` 在它**之后** ⟹ 指针计数恒 0 (前两轮补 4 处仍失败的真因);
         ④ `类型序/标签名` 未**跨逗号续行继承** ⟹ `*a, *b` 的第二个 `b` 拿不到类型序 (主路径用 `满七`)。
   症状: `default_notes_tree.first_non_note->next` 报 `A9001 类型 'notes_tree' 的字段 'first_non_note'
         不是 结构体/联合, 不能取成员` (报错点 L7963, 判据 = 字段 `毛利` < 0)。
   实测: git `notes.h` L51 `extern struct notes_tree { struct non_note *first_non_note, *prev_non_note; ... } default_notes_tree;`
   本锚: extern 内联定义 + 逗号续行指针字段 + 链式访问。真定义用**零初始化**(避开与 extern 声明的
         初始化器冲突 —— 那是本缺陷的邻近问题, 另记)。
   ⚠️ 邻近未修: `extern X {...} v;` **带初始化**的真定义会崩; `X {...} v = {...};`(无 extern) 顶层解析中断。 */
// @EXPECTED out:a=7 b=9 d=1
// @EXPECTED out:c=9
// @EXPECTED exit:0
#include <stdio.h>

struct nn { int m; };
static struct nn n1 = { 7 };
static struct nn n2 = { 9 };

extern struct nt {                     /* extern + 内联定义 (P0-21 病灶) */
	int d;
	struct nn *a, *b;              /* 逗号续行 · 指针字段 */
} g;

struct nt g;                           /* 真定义 (零初始化) */

static struct nn *pick(int k)
{
	return k ? g.a : g.b;          /* 指针字段 作 三目 分支 */
}

int main(void)
{
	g.d = 1;
	g.a = &n1;
	g.b = &n2;
	printf("a=%d b=%d d=%d\n", g.a->m, g.b->m, g.d);
	printf("c=%d\n", pick(0)->m);
	return 0;
}
