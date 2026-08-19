/* 复现6: static const char * const arr[] 的 sizeof 算错 (应为 2*8=16, 被算成 24) */
#include <stdio.h>

static const char * const special_refs[] = {
	"FETCH_HEAD",
	"MERGE_HEAD",
};

int main(void)
{
	int n = (int)(sizeof(special_refs) / sizeof(special_refs[0]));
	printf("ARRAY_SIZE=%d sizeof=%d (expect 2, 16)\n", n, (int)sizeof(special_refs));
	if (n != 2) {
		printf("FAIL: sizeof const ptr array wrong\n");
		return 1;
	}
	printf("OK: sizeof const ptr array works\n");
	return 0;
}
