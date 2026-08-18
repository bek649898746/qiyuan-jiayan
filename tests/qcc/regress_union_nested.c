#include <stdio.h>
#include <stddef.h>

struct cb {
	const char *buf;
	size_t len;
	size_t pos;
};

struct cb2 {
	int file;
	struct cb buf;
};

union cu {
	int file;
	struct { const char *buf; size_t len; size_t pos; } buf;
};

int main(void)
{
	printf("struct cb: buf=%d len=%d pos=%d size=%d\n",
	       (int)offsetof(struct cb, buf), (int)offsetof(struct cb, len),
	       (int)offsetof(struct cb, pos), (int)sizeof(struct cb));
	printf("struct cb2: buf=%d size=%d\n",
	       (int)offsetof(struct cb2, buf), (int)sizeof(struct cb2));
	printf("union cu: buf=%d len=%d pos=%d size=%d\n",
	       (int)offsetof(union cu, buf.buf), (int)offsetof(union cu, buf.len),
	       (int)offsetof(union cu, buf.pos), (int)sizeof(union cu));
	return 0;
}
