#include <stdio.h>
#include <stddef.h>

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
	printf("sizeof=%d\n", (int)sizeof(struct CS));
	printf("buf_off=%d\n", (int)offsetof(struct CS, u.buf.buf));
	printf("len_off=%d\n", (int)offsetof(struct CS, u.buf.len));
	printf("pos_off=%d\n", (int)offsetof(struct CS, u.buf.pos));
	printf("name_off=%d\n", (int)offsetof(struct CS, name));
	printf("linenr_off=%d\n", (int)offsetof(struct CS, linenr));
	printf("do_fgetc_off=%d\n", (int)offsetof(struct CS, do_fgetc));
	return 0;
}
