// @EXPECTED exit:0
// @EXPECTED out:OK
/* 回归 2026-08-18: struct 指针 cast 赋值 — `struct T *out = (struct T*)x;`
   原被 8838 行 struct 值拷贝分支误判 (out 的 st_idx>=0 且 sz>8, RHS 是 no-op cast
   变量节点 nt=1) → 从参数槽拷 24 字节 → out 从未写入 → files_downcast (refs.c)
   返回垃圾指针 → git init 崩。修复: 拷贝分支加 var_pesz==0 守卫 (指针变量排除)。 */
#include <stdio.h>

struct ref_store { int be; int store_flags; };
struct files_ref_store { struct ref_store base; int x; int y; };

static struct files_ref_store *down3(struct ref_store *ref_store, unsigned int required_flags, const char *caller)
{
    struct files_ref_store *out;
    if (ref_store->be != 42)
        return (struct files_ref_store*)0;
    out = (struct files_ref_store *)ref_store;
    return out;
}

static int down4(struct ref_store *ref_store)
{
    struct files_ref_store *refs;
    refs = (struct files_ref_store *)ref_store;
    return refs->x;
}

int main(void)
{
    struct files_ref_store fs;
    fs.base.be = 42;
    fs.x = 7;
    fs.y = 9;
    int v4 = down4((struct ref_store*)&fs);
    if (v4 != 7) { printf("FAIL down4=%d\n", v4); return 1; }
    struct files_ref_store *r = down3((struct ref_store*)&fs, 0, "main");
    if (r != &fs) { printf("FAIL down3 ptr\n"); return 1; }
    if (r->x != 7 || r->y != 9) { printf("FAIL down3 fields\n"); return 1; }
    printf("OK\n");
    return 0;
}
