// @EXPECTED exit:0
// @EXPECTED out:ok
// regress: 成员链指针解引用宽度 (fix 2026-08-19)
//   `*arg = *++p->argv` — *p->argv 是 const char* (8 字节指针值),
//   原 case-12 对成员链 pnode 不解析宽度 → el=0 → movzbl 字节加载
//   → *arg 只取指针低字节 (56) → git commit do_get_value 读垃圾 arg
//   → strbuf_addstr(sb, 56) 崩
#include <stdio.h>

struct ctx {
    const char **argv;
    const char **out;
    int argc;
};

static const char *get_arg(struct ctx *p, const char **arg)
{
    if (p->argc > 1) {
        p->argc--;
        *arg = *++p->argv;
    } else {
        *arg = 0;
    }
    return *arg;
}

int main(void)
{
    const char *a0 = "one", *a1 = "two", *a2 = "three";
    const char *av[4] = { a0, a1, a2, 0 };
    struct ctx c;
    c.argv = av;
    c.argc = 3;
    const char *r;
    const char *got = get_arg(&c, &r);
    if (got != a1 || r != a1) { printf("arg1 fail: %p %p\n", (void *)got, (void *)r); return 1; }
    if (c.argc != 2) { printf("argc fail %d\n", c.argc); return 2; }
    get_arg(&c, &r);
    if (r != a2) { printf("arg2 fail\n"); return 3; }
    printf("ok\n");
    return 0;
}
