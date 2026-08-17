// @EXPECTED exit:0
// @EXPECTED out:1
// regress: struct Tag *cmd 函数参数 — v1 曾编成外部 UND cmd (cat-file.c 链接失败)
#include <stdio.h>

struct queued_cmd {
    const char *line;
    int (*fn)(int, const char *, void *, void *);
};
typedef struct queued_cmd queued_cmd;

static int fn0(int a, const char *b, void *c, void *d) { return a; }

static int dispatch_calls(struct queued_cmd *cmd, int nr) {
    int r = 0;
    for (int i = 0; i < nr; i++)
        r += cmd[i].fn(i, cmd[i].line, 0, 0);
    return r;
}

int main(void) {
    struct queued_cmd q[2] = { {0, 0}, {0, 0} };
    struct queued_cmd call = {0};
    call.line = "x";
    call.fn = fn0;
    q[0] = call;
    q[1] = call;
    printf("%d\n", dispatch_calls(q, 2));
    return 0;
}
