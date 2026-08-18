#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int iskeychar(int c)
{
	return isalnum(c) || c == '-' || c == '_';
}

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
		if (!dot || i > baselen) {
			if (!iskeychar(c) || (i == baselen + 1 && !isalpha(c))) {
				printf("invalid key\n");
				return 1;
			}
		}
		if (c == '.')
			c = '\0';
		else
			c = tolower(c);
		store_key[i] = c;
	}
	store_key[i] = 0;
	printf("store_key len=%d bytes:", (int)strlen(store_key));
	for (i = 0; i < 16; i++)
		printf(" %02x", (unsigned char)store_key[i]);
	printf("\n");
	if (strlen(store_key) != 4 || strcmp(store_key, "core") != 0) {
		printf("FAIL\n");
		return 1;
	}
	printf("OK\n");
	return 0;
}
