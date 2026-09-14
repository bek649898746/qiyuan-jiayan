/* feat_c_ret_agg_ok.c — C 兼容模式: `return <聚合变量>;` **不是所有权移动** (P0-15 根治 2026-09-14)
   病灶: `返移节` (return=移动) **无 甲言模式 门控** → 编译纯 C 源时, `return <聚合变量>;`
         也 被 标 已移 → 同函数 后续分支 (含 goto 目标) 读该变量 → A3101 假阳。
         (A29 只修了 **非聚合** 豁免 —— `return 标量/指针` 不算移动; 聚合 仍 误伤。)
   实测: git `attr.c` 的 `parse_attr_line`: `struct match_attr *res` + `return res;`
         + `fail_return:` 分支 → A3101「变量 'res' 已移动」。
   本锚: 复刻该形态 (return + goto 目标分支读), C 模式 下必须 编译通过 且 行为与 gcc 一致。 */
// @EXPECTED out:r=7 neg=1 fail=0
// @EXPECTED exit:0
#include <stdio.h>

struct ma { int v; };
static struct ma g_store;

static struct ma *parse(int n) {
	struct ma *res = NULL;
	if (n < 0)
		goto fail_return;
	res = &g_store;
	res->v = n;
	return res;               /* C: 返回 不 使 res 失效 */
fail_return:
	if (res)
		res->v = -1;          /* 原 此处 报 A3101 (读已移 res) */
	return NULL;
}

int main(void) {
	struct ma *p = parse(7);
	printf("r=%d", p ? p->v : -1);
	printf(" neg=%d", parse(-1) == NULL);
	printf(" fail=%d\n", g_store.v == -1);
	return 0;
}
