// @EXPECTED exit:0
// @EXPECTED out:16-24
#include <stdio.h>

struct strbuf {
    size_t alloc;
    size_t len;
    char *buf;
};

int main(void) {
    printf("%d-%d\n",
           (int)((char *)&((struct strbuf *)0)->buf - (char *)((struct strbuf *)0)),
           (int)sizeof(struct strbuf));
    return 0;
}
