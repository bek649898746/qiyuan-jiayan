/* 复现6b: 函数内局部 static const char * const arr[] 带 C99 尾逗号 → sizeof 算错 (fix 2026-08-19)
   git refs.c is_special_ref: { "FETCH_HEAD", "MERGE_HEAD", } 尾逗号被算成 3 元素 → sizeof=24
   → ARRAY_SIZE=3 → special_refs[2] 越界 → strcmp 崩 (git init) */
#include <stdio.h>

static int is_special_ref(const char *refname)
{
	static const char * const special_refs[] = {
		"FETCH_HEAD",
		"MERGE_HEAD",
	};
	int i;

	for (i = 0; i < (int)(sizeof(special_refs) / sizeof(special_refs[0])); i++)
		if (!strcmp(refname, special_refs[i]))
			return 1;
	return 0;
}

int main(void)
{
	if (is_special_ref("FETCH_HEAD") != 1) { printf("FAIL: FETCH_HEAD\n"); return 1; }
	if (is_special_ref("MERGE_HEAD") != 1) { printf("FAIL: MERGE_HEAD\n"); return 1; }
	if (is_special_ref("refs/heads/main") != 0) { printf("FAIL: not-special\n"); return 1; }
	printf("OK: local static const char* const [] trailing-comma ARRAY_SIZE works\n");
	return 0;
}
// @EXPECTED exit:0
// @EXPECTED out:OK: local static const char* const [] trailing-comma ARRAY_SIZE works
