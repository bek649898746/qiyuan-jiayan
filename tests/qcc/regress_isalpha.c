#include <stdio.h>
#include <ctype.h>

int main(void)
{
	int r = isalpha('A');
	int r2 = isalpha('1');
	printf("%d %d\n", r, r2);
	if (r != 1 || r2 != 0) {
		printf("FAIL\n");
		return 1;
	}
	printf("OK\n");
	return 0;
}
