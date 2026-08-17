// @EXPECTED exit:0
// @EXPECTED out:8-2-1
// bug#23: 指针字段成员 ++/-- 未按元素大小缩放 (ctx->argv++ char** → +1 而非 +8)
// git parse_options_start_1 ctx->argv++ 错位 → parse_options_step 读错位指针崩
#include <stdio.h>

struct ctx { int argc; char **argv; };

static void f(struct ctx *c) {
    c->argc--;
    c->argv++;
}

int main(void) {
    char *arr[4] = { "a", "b", "c", 0 };
    struct ctx c = { 3, arr };
    long long before = (long long)c.argv;
    f(&c);
    long long moved = (long long)c.argv - before;
    int ac1 = c.argc;
    f(&c);
    int ac2 = c.argc;
    printf("%lld-%d-%d\n", moved, ac1, ac2);
    return 0;
}
