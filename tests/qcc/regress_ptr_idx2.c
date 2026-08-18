#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
	const char *key = "core";
	char *store_key = malloc(64);
	char **out = &store_key;
	int i;
	/* 读取: (*out)[i] */
	for (i = 0; key[i]; i++)
		store_key[i] = (*out)[i];
	store_key[i] = 0;
	printf("read=%s\n", store_key);
	/* 写入: (*out)[i] = x */
	for (i = 0; key[i]; i++)
		(*out)[i] = key[i];
	(*out)[i] = 0;
	printf("write=%s\n", store_key);
	if (strcmp(store_key, "core") != 0) {
		printf("FAIL\n");
		return 1;
	}
	printf("OK\n");
	return 0;
}
