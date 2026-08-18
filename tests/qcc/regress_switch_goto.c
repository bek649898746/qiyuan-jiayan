/* 回归: switch case 内含 goto 标签跳转 (check_refname_component 结构)
   根因风险: fall-through 追加 (ft2) 可能把后续 case body 追加进 goto case → 跳转错乱 */
#include <stdio.h>

static unsigned char disp_tbl[128] = {0};

static int check_component(const char *refname, int *flags)
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
		case 4:
			return -1;
		}
		last = ch;
	}
out:
	return cp - refname;
}

int main(void)
{
	int i;
	for (i = 'a'; i <= 'z'; i++) disp_tbl[i] = 0;
	disp_tbl['/'] = 1;
	disp_tbl['~'] = 4;
	{
		int flags = 0;
		int len = check_component("refs/heads/master", &flags);
		printf("len=%d\n", len);
		if (len != 4) { printf("FAIL: expected 4\n"); return 1; }
	}
	{
		int flags = 0;
		int len = check_component("ab~cd", &flags);
		printf("len2=%d\n", len);
		if (len != -1) { printf("FAIL: expected -1\n"); return 1; }
	}
	printf("OK\n");
	return 0;
}
