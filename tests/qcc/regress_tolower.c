#include <stdio.h>
#include <ctype.h>

int main(void)
{
	int r = tolower('A');
	int r2 = tolower('c');
	int r3 = tolower('x');
	printf("%d %d %d\n", r, r2, r3);
	if (r != 'a' || r2 != 'c' || r3 != 'x') {
		printf("FAIL\n");
		return 1;
	}
	printf("OK\n");
	return 0;
}
