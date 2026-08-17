// @EXPECTED exit:0
// @EXPECTED out:val=hello pval=hello
// bug#24: 多级箭头链取址 &o->m->in->buf — case-15 nested 分支 fsz==8 无条件解引用末指针字段
// (值上下文 o->m->in->buf 应得 char* 指针值; 取址上下文 &o->m->in->buf 应得 buf 字段地址)
// 原实现取址也 mov rax,[rax] → check() 收到 buf 的"值"("hello" 字符串地址) 当指针解引用 → SEGV
// git repo_set_gitdir → expand_base_dir(&...->path) 传 NULL 崩
#include <stdio.h>

struct inner { char *buf; };
struct mid { struct inner *in; };
struct outer { struct mid *m; };

static int check(char **p) {
    return *p != 0;
}

static int g(struct outer *o) {
    char *val = o->m->in->buf;    /* value context: char* 指针值 */
    char **addr = &o->m->in->buf; /* address context: buf 字段地址 */
    return printf("val=%s pval=%s\n", val, *addr);
}

int main(void) {
    struct inner i = { "hello" };
    struct mid m = { &i };
    struct outer o = { &m };
    return g(&o) ? 0 : 1;
}
