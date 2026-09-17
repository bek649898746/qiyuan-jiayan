// @EXPECTED out:PASS
#include <stdio.h>

/* C74 补锚 — **局部** 括号 指针数组 声明 `结构体 T *(名[N]);` (第 74 站, 2026-09-17)
 *
 * 病灶: 函数内 该形态 由 L8770 分支处理 (2026-08-23 为 builtin_diff_tree 所加),
 *       它设了 型序标/指深/指内, 而 **漏设 `指元素尺寸`**;
 *       结构体元素赋值守卫 以 `变量指元素尺寸 == 0` 区分「结构体值数组」与「指针数组」
 *       ⟹ `结构体 T *(a[4]); a[1] = &y;` 走 **结构体值 深拷贝**
 *       (seed 反汇编: lea &y / mov (%rax),%rcx / mov %rcx,(%rbx) — 多读一层)
 *       ⟹ a[1] 收到 y 的 (未初始化) 内容 而非地址 ⟹ `a[1]->字段` 空解引用 崩 0xC0000005.
 * 同族先例: L8839 (2026-08-31 s21, 无括号路径 `结构体 S0 *数组[1]` 同一误判)
 * 真实载体: grep.c L707 `struct grep_expr *(header_group[GREP_HEADER_FIELD_MAX]);`
 *           builtin/diff.c L175 `const struct object_id *(oid[2]);`
 * 修法: L8785 的 `若 (是否指标) { ... }` 块内补 `变量表[变量数 - 1].指元素尺寸 = 8;`
 */

struct pbc {
    int tree_data;
    int oid;
};

/* 函数外 对照 (C72 已修, 本轮确认 未回归) */
static struct pbc *(全局[2]);
static struct pbc g0, g1;

/* ① 局部 括号 指针数组: 取地址 赋值 + 逐元素 独立寻址 + 解引用 写回 */
static int 局部括号(void) {
    struct pbc *(a[4]);
    struct pbc x, y, z, w;
    a[0] = &x; a[1] = &y; a[2] = &z; a[3] = &w;
    y.tree_data = 9;
    if (a[1]->tree_data != 9) return 1;      /* 取址 赋值 若是 深拷贝 → 此处 崩/错值 */
    a[1]->oid = 41;
    if (y.oid != 41) return 2;               /* a[1] 必须 与 &y 同 一 对象 */
    if (y.tree_data != 9) return 3;          /* 相邻 字段 仍 完好 */
    return 0;
}

/* ② 局部 括号 指针数组: 逐元素 独立寻址 (元素 步长 必须 = 8) */
static int 局部括号步长(void) {
    struct pbc *(b[4]);
    struct pbc v0, v1, v2, v3;
    int i;
    b[0] = &v0; b[1] = &v1; b[2] = &v2; b[3] = &v3;
    for (i = 0; i < 4; i++) b[i]->tree_data = 100 + i;
    for (i = 0; i < 4; i++) if (b[i]->tree_data != 100 + i) return 4 + i;
    if (v3.tree_data != 103) return 9;       /* 无 互相 覆盖 */
    return 0;
}

/* ③ 局部 静态 括号 指针数组 (隔排 路径) */
static int 局部静态(void) {
    static struct pbc *(c[3]);
    static struct pbc s0, s1, s2;
    c[0] = &s0; c[1] = &s1; c[2] = &s2;
    c[2]->tree_data = 7;
    if (s2.tree_data != 7) return 10;
    if (c[2]->tree_data != 7) return 11;
    return 0;
}

/* ④ 对照: 局部 无括号 指针数组 (本来就正确) */
static int 局部无括号(void) {
    struct pbc *d[2];
    struct pbc u;
    d[0] = &u;
    u.tree_data = 5;
    if (d[0]->tree_data != 5) return 12;
    return 0;
}

int main(void) {
    int r;
    r = 局部括号();       if (r) return r;
    r = 局部括号步长();   if (r) return r;
    r = 局部静态();       if (r) return r;
    r = 局部无括号();     if (r) return r;

    全局[0] = &g0;      /* 全局 对照 (C72 路径) */
    全局[1] = &g1;
    全局[1]->tree_data = 6;
    if (g1.tree_data != 6) return 13;
    if (全局[0]->tree_data != 0) return 14;

    printf("PASS\n");
    return 0;
}
