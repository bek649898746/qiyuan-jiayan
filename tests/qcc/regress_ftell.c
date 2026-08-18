#include <stdio.h>

int main(void)
{
	FILE *f = fopen("_ftell_test.txt", "wb");
	fputs("[core]\n\trepositoryformatversion = 0\n", f);
	fclose(f);

	f = fopen("_ftell_test.txt", "rb");
	long p0 = ftell(f);
	int c1 = fgetc(f);
	long p1 = ftell(f);
	fgetc(f);
	fgetc(f);
	long p3 = ftell(f);
	fclose(f);

	printf("%ld %ld %ld\n", p0, p1, p3);
	if (p0 != 0 || p1 != 1 || p3 != 3) {
		printf("FAIL\n");
		return 1;
	}
	printf("OK\n");
	return 0;
}
