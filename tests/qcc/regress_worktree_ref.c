#include <stdio.h>
#include <string.h>

enum { CURR = 0, MAIN = 1, OTHER = 2 };

static inline int skip_prefix(const char *str, const char *prefix,
			      const char **out)
{
	do {
		if (!*prefix) {
			*out = str;
			return 1;
		}
	} while (*str++ == *prefix++);
	return 0;
}

static int wt_name_len(const char *name, const char **slash)
{
	int len;
	for (len = 0; name[len] && name[len] != '/'; len++)
		;
	if (name[len] == '/') {
		*slash = name + len;
		return 1;
	}
	*slash = NULL;
	return 0;
}

static int parse_wt(const char *refname, const char **wtname, int *wtname_len,
		    const char **bare_refname)
{
	const char *slash;

	if (skip_prefix(refname, "refs/", &slash)) {
		if (!wt_name_len(slash, &slash)) {
			*wtname = NULL;
			*wtname_len = 0;
			*bare_refname = refname;
		} else {
			*wtname = refname + 5;
			*wtname_len = slash - *wtname;
			*bare_refname = slash;
		}
		return CURR;
	}
	*wtname = refname;
	*wtname_len = (int)strlen(refname);
	*bare_refname = NULL;
	return OTHER;
}

int main(void)
{
	const char *refname = "refs/heads";
	const char *wtname = (const char*)0xdeadbeef, *bare = (const char*)0xdeadbeef;
	int wtlen = -1;
	int t = parse_wt(refname, &wtname, &wtlen, &bare);
	printf("type=%d wtname=%p wtlen=%d bare=%p\n", t, (void*)wtname, wtlen, (void*)bare);
	if (t != CURR) {
		printf("FAIL: expected CURR(0) got %d\n", t);
		return 1;
	}
	if (wtname != NULL || wtlen != 0 || bare != refname) {
		printf("FAIL: wtname=%p wtlen=%d bare=%p (expect NULL/0/refname)\n",
		       (void*)wtname, wtlen, (void*)bare);
		return 2;
	}
	printf("OK\n");
	return 0;
}
