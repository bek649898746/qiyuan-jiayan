// @EXPECTED exit:0
// @EXPECTED out:hello-ok
#include <stdio.h>

struct Ctx {
    const char **out;
    const char **argv;
    int cpidx;
};

int collect(struct Ctx *ctx, const char *val) {
    ctx->out[ctx->cpidx++] = ctx->argv[0];
    return ctx->cpidx;
}

int main(void) {
    const char *args[2] = { "hello", "world" };
    const char *outbuf[2] = { 0, 0 };
    struct Ctx c;
    c.out = outbuf;
    c.argv = args;
    c.cpidx = 0;
    int n = collect(&c, "x");
    printf("%s-%s\n", outbuf[0], n == 1 ? "ok" : "bad");
    return 0;
}
