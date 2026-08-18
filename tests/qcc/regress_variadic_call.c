#include <stdio.h>
#include <string.h>

static const char *cat(const char *fmt, ...) { return fmt; }

int main(void)
{
	char *ref;
	ref = (char*)cat("refs/heads/%s", "master");
	printf("ref=%s\n", ref);
	return 0;
}
