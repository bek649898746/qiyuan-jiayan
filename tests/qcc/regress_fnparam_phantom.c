/* 复现: refs_verify_refname_available 参数错位 — const struct 指针参数 + 5 参函数
   @EXPECTED
   exit: 0
   out: p1=world p2=hello ok
   */
#include <stdio.h>

struct string_list;      /* 前向声明 */

struct strbuf_jy { char *buf; int len; };

static void check(int n, const void *a, const void *b)
{
    if (n == 0) printf("p1=%s ", (const char *)a);
    else printf("p2=%s ", (const char *)a);
}

int verify(struct strbuf_jy *refs, const char *refname,
           const struct string_list *extras,
           const struct string_list *skip,
           struct strbuf_jy *err)
{
    check(0, refname, 0);       /* 第一参数应收到 refname */
    check(1, (const char *)extras, 0);  /* 第二参数应收到 extras */
    return 0;
}

int main(void)
{
    struct strbuf_jy s;
    s.buf = (char *)"root";
    return verify(&s, "world", (const struct string_list *)"hello", 0, 0);
}
