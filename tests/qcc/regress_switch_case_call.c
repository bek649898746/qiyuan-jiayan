#include <stdio.h>
#include <string.h>

enum ref_wt_type {
	REF_WT_CURRENT, /* 0 */
	REF_WT_MAIN,    /* 1 */
	REF_WT_OTHER,   /* 2 */
	REF_WT_SHARED,  /* 3 */
};

static void build(char *out, int t, const char *gitdir, const char *gitcommondir,
		  const char *wtname, int wtname_len, const char *bare)
{
	switch (t) {
	case REF_WT_CURRENT:
		sprintf(out, "%s/%s", gitdir, bare);
		break;
	case REF_WT_OTHER:
		sprintf(out, "%s/worktrees/%.*s/%s", gitcommondir, wtname_len, wtname, bare);
		break;
	case REF_WT_SHARED:
	case REF_WT_MAIN:
		sprintf(out, "%s/%s", gitcommondir, bare);
		break;
	}
}

int main(void)
{
	char out[256] = "";
	build(out, 3, "GITDIR", "GITCOMMONDIR", "wt", 2, "refs/heads");
	printf("out=%s\n", out);
	if (strcmp(out, "GITCOMMONDIR/refs/heads") != 0) {
		printf("FAIL: got '%s'\n", out);
		return 1;
	}
	printf("OK\n");
	return 0;
}
