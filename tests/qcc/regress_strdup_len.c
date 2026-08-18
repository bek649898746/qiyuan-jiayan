#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void)
{
	const char *s = "core.filemode";
	char buf[64];
	strcpy(buf, s);
	printf("buf=%s len=%d\n", buf, (int)strlen(buf));

	char *d = strdup(s);
	printf("dup=%s len=%d\n", d, (int)strlen(d));
	free(d);

	if (strcmp(buf, s) != 0 || strcmp(d ? d : "", s) != 0) {
		printf("FAIL\n");
		return 1;
	}
	printf("OK\n");
	return 0;
}
