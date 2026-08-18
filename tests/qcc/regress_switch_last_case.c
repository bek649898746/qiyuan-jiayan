#include <stdio.h>

enum ref_wt_type {
	REF_WT_CURRENT, /* 0 */
	REF_WT_MAIN,    /* 1 */
	REF_WT_OTHER,   /* 2 */
	REF_WT_SHARED,  /* 3 */
};

int pick(int t)
{
	switch (t) {
	case REF_WT_CURRENT:
		return 10;
	case REF_WT_OTHER:
		return 30;
	case REF_WT_MAIN:
		return 20;
	case REF_WT_SHARED:
		return 40;
	}
	return -1;
}

int main(void)
{
	int r = pick(3);
	printf("pick(3)=%d\n", r);
	if (r != 40) {
		printf("FAIL: expected 40 got %d\n", r);
		return 1;
	}
	printf("OK\n");
	return 0;
}
