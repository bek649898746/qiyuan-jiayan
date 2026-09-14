/* feat_c_top_inline_struct_init_ok.c — 顶层 内联 结构体 定义 + 花括 初始化器 (A75 根治 2026-09-14)
   病灶: 顶层 `结构体 X { ... } 变量 = { ... };` (内联定义 + 花括初始化) —— 值布 契约 = 进入时 词位 在
         `{` 之后 (同 L9174 注释 / L4969 嵌套调用点), 但 L16784 只 `词位++` 一次 (消费 `=`) ⟹ 词位 停 在 `{`
         ⟹ 值布 首字段 走「opaque 花括 跳配平」把 `{ 值 }` 整体跳过 (值 丢失) ⟹ 域序 溢出 ⟹
         「全字段已消费」分支 用 赋值级() 解析 下一条 顶层声明 并 手动跳分隔符 ⟹ **吞掉后续 (main 丢失,
         报「未找到 主 入口」)**。
   本锚: 标量结构体 / 多字段 / 指针字段 / 指定初始化 / union / static / 后续声明不被吞。
   说明: 旧种子 (89476ACC) 上本锚 **编译失败** (顶层解析中断); 修复后 与 gcc 逐字节一致。 */
// @EXPECTED out:v=1,7
// @EXPECTED out:w=4,5,6
// @EXPECTED out:t=0,9
// @EXPECTED out:z=7
// @EXPECTED out:uu=3
// @EXPECTED out:after=0
// @EXPECTED exit:0
#include <stdio.h>

struct nn { int m; };
static struct nn n1 = { 7 };

struct q { int d; struct nn *p; } v = { 1, &n1 };
struct r { int a, b, c; } w = { 4, 5, 6 };
struct s { int a, b; } t = { .b = 9 };
static struct u { int m; } z = { 7 };
union un { int i; int j; } uu = { 3 };
struct q after;

int main(void)
{
    printf("v=%d,%d\n", v.d, v.p->m);
    printf("w=%d,%d,%d\n", w.a, w.b, w.c);
    printf("t=%d,%d\n", t.a, t.b);
    printf("z=%d\n", z.m);
    printf("uu=%d\n", uu.i);
    printf("after=%d\n", after.d);
    return 0;
}
