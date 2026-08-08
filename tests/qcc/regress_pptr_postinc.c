// @EXPECTED exit:0
// 回归: (expr)++ 后缀 — 解析器曾吞掉括号表达式后的 ++/-- (fix 2026-08-08)
// 根因: prim() 括号后缀链无 PP/MM 分支 → `(*p)++` 只编译出 `*p` 加载, 不自增不回写
// 影响: fn_macro_expand_to 的 `out[(*o)++]` 索引不推进 → 镜像宏展开为空
#include <stdio.h>
int main(void) {
    int x = 5;
    int *p = &x;
    int a[3];
    a[1] = 7;

    (*p)++;                  /* 括号解引用后缀自增 */
    if (x != 6) return 1;
    (*p)--;
    if (x != 5) return 2;

    (a[1])++;                /* 括号数组元素后缀自增 */
    if (a[1] != 8) return 3;
    (a[1])--;
    if (a[1] != 7) return 4;

    /* 宏展开体内使用 (*o)++ 索引推进 */
    #define FILL(dst, n) { int _k; for (_k = 0; _k < (n); _k++) (dst)[_k] = _k + 100; }
    FILL(a, 3);
    if (a[0] != 100 || a[1] != 101 || a[2] != 102) return 5;

    printf("pptr-postinc-ok\n");
    return 0;
}
