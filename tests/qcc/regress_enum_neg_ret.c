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
