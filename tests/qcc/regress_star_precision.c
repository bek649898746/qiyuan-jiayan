// @EXPECTED exit:0
// @EXPECTED err:hint: Using 'master' as the name
// 2026-10-23 启元审计: 断言由 out: 改为 err: —— 下面第 13 行明确 fprintf(stderr, ...)
//   模拟 git init 把 hint 打到 stderr 的真实行为，而 out: 只查 stdout ⟹ 恒红。
//   本测试只在 nightly_full（schedule 触发）里跑，长期被掩盖（CI run #186 唯一 FAIL）。
//   编译器行为本身完全正确: stdout='OK', stderr="hint: Using 'master' as the name"。
#include <stdio.h>
#include <string.h>
/* 回归: fprintf %.*s 星号精度 (fix 2026-08-18)
   根因: `*` 当未知说明符跳过 → 参数错位 → 后续 %s 把 int 精度当指针 → SEGV */
int main(void)
{
	const char *cp = "Using 'master' as the name";
	const char *np = strchr(cp, '\n');
	if (!np)
		np = cp + strlen(cp);
	fprintf(stderr, "%shint:%s%.*s%s\n", "", (np == cp) ? "" : " ", (int)(np - cp), cp, "");
	printf("OK\n");
	return 0;
}
