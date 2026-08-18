#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
	char *store_key = malloc(64);
	char **out = &store_key;
	(*out)[1] = 'X';
	printf("%d\n", (int)store_key[1]);
	return 0;
}
