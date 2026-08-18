#include <stdio.h>

struct strbuf { size_t alloc; size_t len; char *buf; };

struct CS;
typedef int (*fgetc_t)(struct CS *c);
typedef int (*ungetc_t)(int c, struct CS *conf);
typedef long (*ftell_t)(struct CS *c);

struct CS {
	struct CS *prev;
	union {
		int file;
		struct { const char *buf; size_t len; size_t pos; } buf;
	} u;
	int origin_type;
	const char *name;
	const char *path;
	int default_error_action;
	int linenr;
	int eof;
	size_t total_len;
	struct strbuf value;
	struct strbuf var;
	unsigned subsection_case_sensitive : 1;
	fgetc_t do_fgetc;
	ungetc_t do_ungetc;
	ftell_t do_ftell;
};

int main(void)
{
	struct CS c;
	c.u.file = 7;
	c.u.buf.len = 42;
	c.u.buf.pos = 99;
	printf("file=%d len=%d pos=%d\n", c.u.file, (int)c.u.buf.len, (int)c.u.buf.pos);
	if (c.u.file != 7 || c.u.buf.len != 42 || c.u.buf.pos != 99) {
		printf("FAIL\n");
		return 1;
	}
	printf("OK\n");
	return 0;
}
