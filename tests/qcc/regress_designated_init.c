// @EXPECTED exit:0
// @EXPECTED out:ok-hello
#include <stdio.h>

struct strbuf {
    unsigned long alloc;
    unsigned long len;
    char *buf;
};

char strbuf_slopbuf[] = "";
#define STRBUF_INIT  { .buf = strbuf_slopbuf }

int main(void) {
    struct strbuf sb = STRBUF_INIT;
    if (sb.buf != strbuf_slopbuf || sb.alloc != 0 || sb.len != 0) {
        printf("bad\n");
        return 1;
    }
    printf("ok-hello\n");
    return 0;
}
