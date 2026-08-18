// @EXPECTED exit:0
// @EXPECTED out:hint: Using 'master' as the name
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
