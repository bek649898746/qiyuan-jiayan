#include <stdio.h>

int main(void)
{
	FILE *f = fopen("_fgetc_test.txt", "wb");
	fputs("[core]\n", f);
	fclose(f);

	f = fopen("_fgetc_test.txt", "rb");
	int c1 = fgetc(f);
	int c2 = fgetc(f);
	int c3 = fgetc(f);
	int c4 = fgetc(f);
	int c5 = fgetc(f);
	int c6 = fgetc(f);
	int c7 = fgetc(f);
	fclose(f);

	printf("%d %d %d %d %d %d %d\n", c1, c2, c3, c4, c5, c6, c7);
	if (c1 != '[' || c2 != 'c' || c3 != 'o' || c4 != 'r' || c5 != 'e' ||
	    c6 != ']' || c7 != '\n') {
		printf("FAIL\n");
		return 1;
	}
	printf("OK\n");
	return 0;
}
