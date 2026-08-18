#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
	const char *key = "core.filemode";
	int baselen = 4;
	char *store_key = malloc(strlen(key) + 1);
	int dot = 0;
	int i;
	for (i = 0; key[i]; i++) {
		unsigned char c = key[i];
		if (c == '.')
			dot = 1;
		if (c == '.')
			c = '\0';
		store_key[i] = c;
	}
	store_key[i] = 0;
	printf("len=%d i=%d bytes:", (int)strlen(store_key), i);
	for (int j = 0; j < 16; j++)
		printf(" %02x", (unsigned char)store_key[j]);
	printf("\n");
	if (strlen(store_key) != 4) {
		printf("FAIL\n");
		return 1;
	}
	printf("OK\n");
	return 0;
}
