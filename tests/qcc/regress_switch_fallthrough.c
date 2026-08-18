/* Regression: switch case-with-body fallthrough to next case (case '\\': C; then default: D)
   Root cause: only EMPTY fall-through cases shared the next case's body; a case WITH a body
   and no break lost the next case's statements -> write_pair escaping wrote one backslash
   instead of two -> value C:\/Users -> parser hit invalid \/ escape -> config rewrite broke. */
#include <stdio.h>

static int esc(char *sb, const char *value)
{
	int n = 0, i;
	for (i = 0; value[i]; i++) {
		switch (value[i]) {
		case '\n':
			sb[n++] = '\\';
			sb[n++] = 'n';
			break;
		case '\\':
			sb[n++] = '\\';
			/* fallthrough */
		default:
			sb[n++] = value[i];
			break;
		}
	}
	sb[n] = 0;
	return n;
}

int main(void)
{
	char sb[128];
	int n = esc(sb, "C:\\Users\\x");
	printf("n=%d sb=%s\n", n, sb);
	if (n != 12 || sb[2] != '\\' || sb[3] != '\\' || sb[9] != '\\' || sb[10] != '\\') {
		printf("FAIL: expected 12 chars with doubled backslashes\n");
		return 1;
	}
	printf("OK\n");
	return 0;
}
