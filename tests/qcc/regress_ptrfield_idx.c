// @EXPECTED exit:0
// @EXPECTED out:1-1-1
// 回归: ptr->指针字段[i] 必须解引用取指针值 + 按指向元素缩放 (fix 2026-08-17)
// 原 case 15 cg_no_deref 对指针字段保留字段地址 + 按字段大小(8)缩放
// → p->buf[i] 编译成 ((char**)p)[i] → 越界读 (git setup.c dir->buf[offset] 崩溃根因)
#include <stdio.h>

struct P { char *name, *help; int *nums; };

static int f(struct P *p, int i) { return p->name[i] == '/'; }
static int g(struct P *p, int i) { return p->help[i] == '!'; }
static int h(struct P *p, int i) { return p->nums[i] == 42; }

int main(void) {
    struct P p;
    char a[8] = "ab/cdef";
    char b[8] = "xy!z";
    int n[4] = { 1, 2, 42, 4 };
    p.name = a; p.help = b; p.nums = n;
    printf("%d-%d-%d\n", f(&p, 2), g(&p, 2), h(&p, 2));
    return 0;
}
