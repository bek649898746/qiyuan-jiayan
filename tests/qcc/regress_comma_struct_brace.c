// @EXPECTED exit:0
// @EXPECTED out:ok=1
// bug#26: 结构体逗号声明 + 花括号初始化器 — `struct strbuf dir = STRBUF_INIT, gitdir = STRBUF_INIT, report = STRBUF_INIT;`
// 原逗号路径用 Nc(d2,expr()) 解析 { → expr() 失败 → 初始化器被吞 + 后续逗号变量未注册
// → 变量 buf=NULL / 调用参数链用上次返回值 → git init/config 崩 (setup.c setup_git_directory_gently)
// 修复: 逗号 struct 变量走 brace_fields 花括号初始化 (局部+全局两条路径)
#include <stdio.h>

struct strbuf {
    unsigned long long alloc;
    unsigned long long len;
    char *buf;
};

static char slopbuf[1] = {0};
#define STRBUF_INIT { 0, 0, slopbuf }

static void strbuf_reset(struct strbuf *sb) {
    sb->len = 0;
    sb->buf[sb->len] = '\0';
}

static int runtest(const char *p) {
    struct strbuf dir = STRBUF_INIT, gitdir = STRBUF_INIT, report = STRBUF_INIT;
    strbuf_reset(&dir);
    strbuf_reset(&gitdir);
    strbuf_reset(&report);
    int ok = (dir.buf == slopbuf && gitdir.buf == slopbuf && report.buf == slopbuf);
    printf("ok=%d\n", ok);
    return ok ? 0 : 1;
}

int main(void) {
    return runtest("ok");
}
