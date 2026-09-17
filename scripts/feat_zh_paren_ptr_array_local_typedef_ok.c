// @EXPECTED out:PASS
#include <stdio.h>

/* C75 补锚 — **typedef** 括号 指针数组 声明 `T *(名[N]);` (第 75 站, 2026-09-17)
 *
 * 病灶: 局部声明区 的 括号数组分支 (L9661) 经 隔排/末午 登记 时 只设了 `指元素尺寸`,
 *       **漏设 `型序标`** (同族先例 L9509 定长数组 显式设;
 *       文件级 同形分支 L17974 亦有 C72 补的 型序标)
 *       ⟹ `T *(a[4]); a[1]->字段` 由 变量型序 查不到 类型 ⟹ 槽4/槽6 静默兜底 (整段不发射).
 * 最小复现: c206.py q5 —— seed 静默 4 (槽4:2 槽6:2), C75 版 静默 0.
 * 与 C74 (第 74 站) 的区别: C74 修的是 `结构体 标签` 形态 (走 L8770, 崩 0xC0000005);
 *       本锚是 **typedef 形态** (走 L9661, 静默兜底)。两者是同一族 (括号分支漏设字段) 的两条路径。
 */

typedef struct { int tree_data; int oid; } T;

/* ① typedef 括号 指针数组: 取址赋值 + 解引用 */
static int 取址(void) {
    T *(a[4]);
    T y;
    a[1] = &y;
    y.tree_data = 9;
    if (a[1]->tree_data != 9) return 1;
    a[1]->oid = 41;
    if (y.oid != 41) return 2;
    return 0;
}

/* ② typedef 括号 指针数组: 逐元素 独立寻址 (元素步长 = 8) */
static int 步长(void) {
    T *(b[4]);
    T v0, v1, v2, v3;
    int i;
    b[0] = &v0; b[1] = &v1; b[2] = &v2; b[3] = &v3;
    for (i = 0; i < 4; i++) b[i]->tree_data = 100 + i;
    for (i = 0; i < 4; i++) if (b[i]->tree_data != 100 + i) return 3 + i;
    if (v3.tree_data != 103) return 8;
    return 0;
}

/* ③ 对照: typedef 无括号 指针数组 (本来就正确) */
static int 无括号(void) {
    T *c[2];
    T u;
    c[0] = &u;
    u.tree_data = 5;
    if (c[0]->tree_data != 5) return 9;
    return 0;
}

int main(void) {
    int r;
    r = 取址();   if (r) return r;
    r = 步长();   if (r) return r;
    r = 无括号(); if (r) return r;
    printf("PASS\n");
    return 0;
}
