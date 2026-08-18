#include <stdio.h>

int main(void)
{
	int r = '\r';
	int n = '\n';
	if (r != 13) {
		printf("FAIL: '\\r' = %d (expected 13)\n", r);
		return 1;
	}
	if (n != 10) {
		printf("FAIL: '\\n' = %d (expected 10)\n", n);
		return 1;
	}
	printf("OK: r=%d n=%d\n", r, n);
	return 0;
}
