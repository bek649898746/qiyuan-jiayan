#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
	const char *key = "core";
	char *store_key = malloc(64);
	char **out = &store_key;
	int i;

	/* 单元素读取: c = (*out)[i] */
	char c0 = (*out)[0];
	printf("c0=%d\n", c0);

	/* 单元素写入: (*out)[1] = 'X' */
	(*out)[1] = 'X';
	printf("s1=%d\n", (int)store_key[1]);

	/* 循环写入 */
	for (i = 0; key[i]; i++)
		(*out)[i] = key[i];
	printf("loop=%d\n", (int)store_key[2]);
	if (store_key[0] != 'c' || store_key[1] != 'o' || store_key[2] != 'r') {
		printf("FAIL: %d %d %d\n", (int)store_key[0], (int)store_key[1], (int)store_key[2]);
		return 1;
	}
	printf("OK\n");
	return 0;
}
