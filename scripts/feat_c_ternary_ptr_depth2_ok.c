/* feat_c_ternary_ptr_depth2_ok.c — 下标 / 成员 的 多级指针 结果 作 三目分支 (P0-20 根治 2026-09-14)
   病灶: `表达式构序` (构值三目的分支类型判定) 的 **下标** 分支是 `返 序七(数)`,
         而 `序七` 对 下标/成员 是「跳过」语义, 返回的是 **基类型序** ——
         根因是 `变量型序(名)` / `字段类型序直(...)` **只给基类型序, 不含指针层级**。
         ⟹ `T **arr` 的 `arr[i]` · `struct S { T **m; }` 的 `s.m[i]` (实为 `T *`)
            被当成 `T` **构值** ⟹ 三目 `cond ? arr[i] : NULL` 左=构值 / 右=常数
            → A9001「结构体值 三目: 两 分支 必须 是 同一 结构体/联合 类型」**假拦**
            (C 里完全合法: 两 分支 都是指针)。
   实测: git `revision.c` / `read-cache.c` (下标基是**成员**节点) + `blame.c` (解引用, P0-17)。
   修法: 新增 `表达式指深()` (递归求表达式指针层级), 解引用 / 下标 两分支统一用它。
   本锚: 变量二级指针下标 / 成员二级指针下标 / 一级指针下标对照, 与 gcc 逐字节对照。 */
// @EXPECTED out:a=7 b=1
// @EXPECTED out:c=7 d=1
// @EXPECTED out:e=7
// @EXPECTED exit:0
#include <stdio.h>
#include <stddef.h>

struct inner { int v; };
struct holder { struct inner **items; };   /* 二级指针字段 */
struct holder1 { struct inner *one; };     /* 一级指针字段 (对照) */

static struct inner i0 = { 7 };
static struct inner *slot[1] = { &i0 };
static struct inner **garr = slot;         /* 二级指针变量 */

static struct inner *pick_var(int n)   { return n ? garr[0] : NULL; }        /* 变量基 · 二级 */
static struct inner *pick_field(struct holder *h, int n)
                                       { return n ? h->items[0] : NULL; }     /* 成员基 · 二级 */
static int           pick_one(struct holder1 *h, int n)
                                       { return (n ? h->one : &i0)->v; }     /* 成员基 · 一级 (对照) */

int main(void)
{
	struct holder  h;  h.items = slot;
	struct holder1 h1; h1.one = &i0;

	printf("a=%d b=%d\n", pick_var(1) ? pick_var(1)->v : -1, pick_var(0) == NULL);
	printf("c=%d d=%d\n", pick_field(&h, 1) ? pick_field(&h, 1)->v : -1, pick_field(&h, 0) == NULL);
	printf("e=%d\n", pick_one(&h1, 1));
	return 0;
}
