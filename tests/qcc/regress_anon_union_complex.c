// @EXPECTED exit:0
// @EXPECTED out:OK
/* 回归 2026-08-18: 复杂匿名 union body (enum/内联匿名 struct/位域/数组) 解析
   — ref-filter.c used_atom 的 union 15+ 成员, 原简化解析在匿名 enum 处脱轨
   → 大段函数被吞 (get_head_description 缺失) → 链接 undefined symbol。
   行为断言: union 成员写读 + union 后字段偏移 + 大 union 成员数组访问。 */
#include <stdio.h>
#include <string.h>

#define COLOR_MAXLEN 70

struct refname_atom { const char *refname; int lstrip; int rstrip; };

struct used_atom {
    int atom_type;
    const char *name;
    int type;
    int source;
    union {
        char color[COLOR_MAXLEN];
        struct align { int align; } align;
        struct {
            int option;
            struct refname_atom refname;
            unsigned int nobracket : 1, push : 1;
        } remote_ref;
        struct {
            int option;
            unsigned int nlines;
        } contents;
        struct {
            int option;
        } raw_data;
    } u;
    int after_u;
};

char *get_head_description(void)
{
    return (char*)"(no branch)";
}

int main(void) {
    struct used_atom a;
    memset(&a, 0, sizeof(a));
    /* union 前面的字段必须各自独立 */
    a.atom_type = 7;
    a.source = 9;
    if (a.atom_type != 7) { printf("FAIL atom_type\n"); return 1; }
    /* 复杂 union 成员写读 */
    a.u.remote_ref.option = 5;
    a.u.remote_ref.refname.lstrip = 3;
    a.u.remote_ref.nobracket = 1;
    a.u.remote_ref.push = 0;
    if (a.u.remote_ref.option != 5) { printf("FAIL remote_ref.option\n"); return 1; }
    if (a.u.remote_ref.refname.lstrip != 3) { printf("FAIL refname.lstrip\n"); return 1; }
    if (a.u.remote_ref.nobracket != 1) { printf("FAIL nobracket\n"); return 1; }
    /* 大 union 成员 (数组) 写读 */
    a.u.color[10] = 'X';
    a.u.color[69] = 'Y';
    if (a.u.color[10] != 'X' || a.u.color[69] != 'Y') { printf("FAIL color\n"); return 1; }
    /* union 之后的字段偏移正确 */
    a.after_u = 0xABCD;
    if (a.after_u != 0xABCD) { printf("FAIL after_u\n"); return 1; }
    /* 函数未被吞 */
    char *d = get_head_description();
    if (strcmp(d, "(no branch)") != 0) { printf("FAIL fn\n"); return 1; }
    printf("OK\n");
    return 0;
}
