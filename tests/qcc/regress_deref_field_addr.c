/* 复现5: &(*e)->next 链遍历 (hashmap find_entry_ptr 场景) — 原编成 e=e no-op 死循环 */
#include <stdio.h>

struct entry {
	struct entry *next;
	int x;
};

static int walk(struct entry **e, int limit)
{
	int c = 0;
	while (*e && c < limit) {
		e = &(*e)->next;
		c++;
	}
	return c;
}

int main(void)
{
	struct entry a, b, c;
	a.next = &b; b.next = &c; c.next = 0;
	struct entry *p = &a;
	int n = walk(&p, 100);
	printf("walked %d nodes (expect 3)\n", n);
	if (n != 3) {
		printf("FAIL: &(*e)->next chain broken\n");
		return 1;
	}
	printf("OK: &(*e)->next chain works\n");
	return 0;
}
