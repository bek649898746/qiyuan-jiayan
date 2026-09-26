// @EXPECTED exit:0      # round 30: 补齐运行期断言 —— 本锚名含 _neg 但语义是【负数 enum 常量】, 不是负例
/* 复现4: enum 负常量 return (ITER_DONE=-2 被编成 -1) */
#include <stdio.h>

enum iterator_ret {
	ITER_OK = 0,
	ITER_ERROR = -1,
	ITER_DONE = -2,
};

static int empty_abort(void)
{
	return ITER_DONE;
}

int main(void)
{
	int r = empty_abort();
	printf("r = %d (expect -2)\n", r);
	if (r != -2) {
		printf("FAIL: ITER_DONE miscompiled\n");
		return 1;
	}
	printf("OK: enum neg const return works\n");
	return 0;
}
