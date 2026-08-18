#include <stdio.h>
#include <string.h>

int main(void)
{
	const char *s = "core.filemode";
	int n = strlen(s);
	printf("strlen=%d\n", n);
	if (n != 13) {
		printf("FAIL: got %d\n", n);
		return 1;
	}
	printf("OK\n");
	return 0;
}
