/* 回归: check_refname_component 完整结构 — switch 前 if + case1 goto out + out 后 memcmp */
#include <stdio.h>
#include <string.h>

static unsigned char disp_tbl[256] = {
	1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 2, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 4,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 0, 4, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 4, 4
};

static const char LOCK_SUFFIX[] = ".lock";
#define LOCK_SUFFIX_LEN 5

static int check_refname_component(const char *refname, int *flags)
{
	const char *cp;
	char last = '\0';

	for (cp = refname; ; cp++) {
		int ch = *cp & 255;
		unsigned char disp = disp_tbl[ch];

		switch (disp) {
		case 1:
			goto out;
		case 2:
			if (last == '.')
				return -1;
			break;
		case 3:
			if (last == '@')
				return -1;
			break;
		case 4:
			return -1;
		case 5:
			if (!(*flags & 0x800))
				return -1;
			*flags &= ~0x800;
			break;
		}
		last = ch;
	}
out:
	if (cp == refname)
		return 0;
	if (refname[0] == '.')
		return -1;
	if (cp - refname >= LOCK_SUFFIX_LEN &&
	    !memcmp(cp - LOCK_SUFFIX_LEN, LOCK_SUFFIX, LOCK_SUFFIX_LEN))
		return -1;
	return cp - refname;
}

int main(void)
{
	int flags = 0;
	int len = check_refname_component("refs/heads/master", &flags);
	printf("len=%d\n", len);
	if (len != 4) { printf("FAIL: expected 4\n"); return 1; }
	printf("OK\n");
	return 0;
}
