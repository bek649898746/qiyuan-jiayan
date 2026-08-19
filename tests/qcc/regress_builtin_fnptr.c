// @EXPECTED exit:0
// @EXPECTED out:fnptr ok
// regress: 函数指针赋值 + 间接调用 (fix 2026-08-19)
//   coff 模式下 builtin (strcmp 等) 取地址 → jyld msvcrt 导入 — git string_list
//   `cmp = list->cmp ? list->cmp : strcmp` 原编成 NULL → call *0 → status SEGV.
//   (单文件模式 builtin 无真实函数体 → 本测试用用户函数; builtin 路径由 git 冒烟验证)
#include <stdio.h>

typedef int (*cmp_fn)(const char *, const char *);

static int my_cmp(const char *a, const char *b)
{
    const char *p = a, *q = b;
    while (*p && *q && *p == *q) { p++; q++; }
    return *p - *q;
}

int main(void)
{
    cmp_fn cmp = my_cmp;      /* bare function name as value */
    if (!cmp) { printf("cmp is NULL\n"); return 1; }
    if (cmp("abc", "abc") != 0) { printf("equal fail\n"); return 2; }
    if (cmp("abc", "abd") >= 0) { printf("less fail\n"); return 3; }
    if (cmp("abd", "abc") <= 0) { printf("greater fail\n"); return 4; }
    printf("fnptr ok\n");
    return 0;
}
