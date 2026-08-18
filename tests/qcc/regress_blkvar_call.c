#include <stdio.h>

static int foo(void) { return 42; }

static int bar(int a, int b) { return a + b; }

int main(void)
{
	if (1) {
		int ref;
		ref = foo();
		printf("A=%d\n", ref);
		if (ref != 42) { printf("FAIL A\n"); return 1; }
	}
	if (1) {
		int ref2;
		ref2 = bar(3, 4);
		printf("B=%d\n", ref2);
		if (ref2 != 7) { printf("FAIL B\n"); return 1; }
	}
	printf("OK\n");
	return 0;
}
