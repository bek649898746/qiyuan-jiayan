/* feat_c_ternary_ptr_depth_ok.c — 多级指针 解引用 作 三目 分支 (P0-17 根治 2026-09-14)
   病灶: `表达式构序` (构值 三目 的 分支 类型 判定) 的 **解引用 分支 丢 指针层级**:
         `变量型序(名)` 只 给 基类型序, **不含 指针层级**; `序七` 对 解引用 是 `返 序七(子0)` (直接 跳过)
         ⟹ `T **q` 的 `*q` (实为 `T *`) 被 当成 `T` **构值** ⟹ 三目 `cond ? *q : NULL`
         左 = 构值 / 右 = 常数 → A9001「结构体值 三目: 两 分支 必须 是 同一 结构体/联合 类型」**假拦**
         (C 里 `cond ? *q : NULL` 完全合法: 两 分支 都是 指针)。
   实测: git `blame.c` L33 `return result ? *result : NULL;` (result = `struct blame_origin **`)。
   本锚: 多级指针 解引用 三目 (含 空指针 常量 分支 + 链式 成员访问), 与 gcc 逐字节对照。 */
// @EXPECTED out:null=1
// @EXPECTED out:v=5
// @EXPECTED out:two=1
// @EXPECTED exit:0
#include <stdio.h>

struct blame_origin { int v; };
static struct blame_origin go1;
static struct blame_origin *g_slot;

static struct blame_origin *get(struct blame_origin **result)
{
	return result ? *result : NULL;   /* result: T** → *result: T* (指针), 非 构值 */
}

int main(void)
{
	struct blame_origin **p = &g_slot;
	printf("null=%d\n", get(p) == NULL);
	g_slot = &go1;
	go1.v = 5;
	printf("v=%d\n", get(p)->v);
	printf("two=%d\n", get(NULL) == NULL);
	return 0;
}
