#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fill(char **out, const char *key)
{
	int i;
	for (i = 0; key[i]; i++)
		(*out)[i] = key[i];
	(*out)[i] = 0;
}

int main(void)
{
	const char *key = "core.filemode";
	char *store_key = malloc(64);
	char *pk = store_key;
	fill(&pk, key);
	printf("len=%d bytes:", (int)strlen(store_key));
	for (int j = 0; j < 14; j++)
		printf(" %02x", (unsigned char)store_key[j]);
	printf("\n");
	if (strcmp(store_key, "core.filemode") != 0) {
		printf("FAIL\n");
		return 1;
	}
	printf("OK\n");
	return 0;
}
