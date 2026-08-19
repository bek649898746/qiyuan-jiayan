// @EXPECTED exit:0
// @EXPECTED out:ok
// regress: 指针后 const 限定符字段 (fix 2026-08-19)
//   `const char * const key` — 指针后的 const 原残留 token → 字段名被当类型吞掉
//   → trace_key 的 key 字段丢失 (fd@0, sizeof=8) → get_trace_fd 读错位 →
//   git add getenv(NULL) → CRT invalid parameter 崩
#include <stdio.h>
#include <stddef.h>

struct trace_key {
    const char * const key;
    int fd;
    unsigned int initialized : 1;
    unsigned int need_close : 1;
};

int main(void)
{
    if (offsetof(struct trace_key, key) != 0) { printf("key off=%d\n", (int)offsetof(struct trace_key, key)); return 1; }
    if (offsetof(struct trace_key, fd) != 8) { printf("fd off=%d\n", (int)offsetof(struct trace_key, fd)); return 2; }
    if (sizeof(struct trace_key) != 16) { printf("sizeof=%d\n", (int)sizeof(struct trace_key)); return 3; }
    struct trace_key t = { "GIT_TRACE", 3, 1, 0 };
    if (t.fd != 3) { printf("fd fail %d\n", t.fd); return 4; }
    if (t.initialized != 1 || t.need_close != 0) { printf("bits fail %d %d\n", t.initialized, t.need_close); return 5; }
    if (t.key[0] != 'G') { printf("key fail\n"); return 6; }
    printf("ok\n");
    return 0;
}
