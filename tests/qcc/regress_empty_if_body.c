/* 复现: if(cond) ; else if 链 — 空语句体被 expr() 返回 -1 → if 空体 codegen 错乱 (fix 2026-08-19)
   git commit.c handle_ignored_arg: `if (!ignored_arg) ; else if (...) ...` 无条件执行 else 链
   → strcmp(NULL, "traditional") SEGV */
#include <stdio.h>

static const char *ignored_arg = 0;

static int handle(const char *s)
{
	int mode = 0;
	if (!ignored_arg)
		; /* default already initialized */
	else if (!s)
		mode = 1;
	else if (s[0] == 'x')
		mode = 2;
	else
		mode = 3;
	return mode;
}

int main(void)
{
	int m = handle("anything");
	printf("mode=%d\n", m);
	if (m != 0) { printf("FAIL: else chain ran with empty if-body\n"); return 1; }
	printf("OK: empty if-body skips else chain\n");
	return 0;
}
// @EXPECTED exit:0
// @EXPECTED out:mode=0
// @EXPECTED out:OK: empty if-body skips else chain
