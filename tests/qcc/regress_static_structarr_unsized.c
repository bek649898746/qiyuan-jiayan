// @EXPECTED exit:0
// @EXPECTED out:3-4-5
// bug#17b: 函数内 static struct 未定长数组 [] = {...} — 需推断元素数并正确分配 .data 槽
// (git fsck.c fsck_describe_object bufs[] 模式; 原 scnt=1 → 越界写坏邻槽)
#include <stdio.h>

char slop[] = "";

struct S {
    char *buf;
    int len;
};

struct S *getb(void) {
    static struct S bufs[] = {
        { slop, 3 },
        { slop, 4 },
        { slop, 5 },
    };
    static int j = 0;
    struct S *sb = &bufs[j];
    j = (j + 1) % 3;
    return sb;
}

int main(void) {
    struct S *c = getb();
    struct S *e = getb();
    struct S *f = getb();
    printf("%d-%d-%d\n", c->len, e->len, f->len);
    return 0;
}
