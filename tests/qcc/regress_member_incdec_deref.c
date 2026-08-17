// @EXPECTED exit:0
// @EXPECTED out:len=4 buf=AB
// bug#21: ++ 目标为 struct 字段 (size_t) 时被误当指针字段解引用 → 崩
// (git strbuf_addch `sb->buf[sb->len++]` — case 23 求 &target 时
//  case 15 的「指针字段作数组基」解引用误触发)
#include <stdio.h>

struct strbuf {
    unsigned long long alloc;
    unsigned long long len;
    char *buf;
};

static void addch(struct strbuf *sb, int c) {
    sb->buf[sb->len++] = c;
    sb->buf[sb->len] = '\0';
}

int main(void) {
    char buf[16] = {0};
    struct strbuf sb = { 16, 0, buf };
    addch(&sb, 'A');
    addch(&sb, 'B');
    printf("len=%llu buf=%s\n", sb.len, sb.buf);
    return 0;
}
