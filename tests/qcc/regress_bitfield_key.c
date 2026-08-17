// @EXPECTED exit:0
// @EXPECTED out:0-0
// 复现: trace_key 位域结构 (initialized:1) 静态初始化 + 访问
#include <stdio.h>

struct trace_key {
    const char * const key;
    int fd;
    unsigned int initialized : 1;
    unsigned int need_close : 1;
};

static struct trace_key trace_default = { .key = "GIT_TRACE" };

int main(void) {
    printf("%d-%d\n", trace_default.fd, trace_default.initialized);
    return 0;
}
