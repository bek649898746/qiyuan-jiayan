#include <stdio.h>

struct InnerA {
	int (*init_db)(void *a, void *b);
	int marker;
};

struct OuterA {
	int base;
	const struct InnerA *c_inner;
	struct InnerA *inner;
	int tail;
};

static int cb(void *a, void *b) { (void)a; (void)b; return 7; }

int H(struct OuterA *o, void *a, void *b)
{
	return o->c_inner->init_db(a, b);
}

int main(void)
{
	struct InnerA inner;
	struct OuterA o;
	inner.init_db = cb;
	o.c_inner = &inner;
	o.inner = &inner;
	int r = H(&o, 0, 0);
	printf("r=%d c_inner_off=%d inner_off=%d\n", r,
	       (int)((char*)&o.c_inner - (char*)&o),
	       (int)((char*)&o.inner - (char*)&o));
	if (r != 7) {
		printf("FAIL: got %d\n", r);
		return 1;
	}
	printf("OK\n");
	return 0;
}
