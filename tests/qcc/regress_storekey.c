#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
	const char *key = "core.filemode";
	char *store_key = malloc(64);
	int i;
	int dot = 0;
	for (i = 0; key[i]; i++) {
		char c = key[i];
		if (c == '.')
			dot = 1;
		if (c == '.')
			c = '\0';
		store_key[i] = c;
	}
	store_key[i] = 0;
	printf("store_key=%s\n", store_key);
	if (strcmp(store_key, "core") != 0) {
		printf("FAIL: got %s\n", store_key);
		return 1;
	}
	printf("OK\n");
	return 0;
}
