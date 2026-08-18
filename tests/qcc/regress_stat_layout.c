#include <stdio.h>
#include <stddef.h>
#include <sys/stat.h>

int main(void)
{
	printf("sizeof=%d\n", (int)sizeof(struct stat));
	printf("st_size_off=%d\n", (int)offsetof(struct stat, st_size));
	printf("st_mode_off=%d\n", (int)offsetof(struct stat, st_mode));
	printf("st_atime_off=%d\n", (int)offsetof(struct stat, st_atime));
	return 0;
}
