// @EXPECTED exit:0
// @EXPECTED out:B9 OK
/* B9 防回归 (2026-09-09): strbuf 布局 + 变参 vsnprintf 纯单文件模式
   (08-17 prelude #18a strbuf/vsnprintf/原型 遗留 — 已随 08-19~24 git 全链修复;
    此测试锁死: 自实现变参收集 → vsnprintf → 结构体字段布局) */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

struct strbuf { char *buf; int len; int alloc; };   /* 布局: buf/len/alloc */

static void sb_fmt(struct strbuf *sb, const char *fmt, ...) {
    char tmp[256];
    va_list ap; va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    int sl = (int)strlen(tmp);
    if (sb->len + sl + 1 > sb->alloc) {
        sb->alloc = sb->len + sl + 1 + 64;
        sb->buf = (char*)malloc(sb->alloc);
    }
    memcpy(sb->buf + sb->len, tmp, sl + 1);
    sb->len += sl;
}

int main(void) {
    struct strbuf sb = {0, 0, 0};
    sb_fmt(&sb, "%d %u %x %c %s %ld", -5, 300u, 255, 'Q', "str", 70000L);
    if (strcmp(sb.buf, "-5 300 ff Q str 70000") != 0) { printf("FAIL1 [%s]\n", sb.buf); return 1; }
    sb.len = 0; if (sb.buf) sb.buf[0] = 0;
    sb_fmt(&sb, "%5d|%-5d|%.3f|%08x", 42, 42, 2.5, 0xAB);
    if (strcmp(sb.buf, "   42|42   |2.500|000000ab") != 0) { printf("FAIL2 [%s]\n", sb.buf); return 1; }
    if (sb.len != (int)strlen("   42|42   |2.500|000000ab")) { printf("FAIL3 len=%d\n", sb.len); return 1; }
    if (sb.alloc < sb.len) { printf("FAIL4 alloc=%d\n", sb.alloc); return 1; }
    printf("B9 OK\n");
    return 0;
}
