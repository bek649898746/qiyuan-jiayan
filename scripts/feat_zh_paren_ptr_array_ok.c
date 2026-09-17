// @EXPECTED out:PASS
#include <stdio.h>

/* C72 补锚 — **括号 指针数组 声明** `结构体 T *(名[N]);` (第 72 站, 2026-09-17)
 *
 * 病灶: 文件级声明 里 该形态 由 L17974 分支处理 (条件 `暂粗 && 左括 && 词变量 && (左方||右括)`),
 *       但 它调用 `隔排` (L3525 新建条目 置 型序标=-1) 后**漏设 类型序标** ——
 *       而 结构体指针数组 分支 (L18177) 显式 `型序标 = 首深`.
 *       ⟹ `名[i]->字段` 由 变量型序 查不到 类型 ⟹ 槽4/槽6 静默兜底 (整段不发射) + 寻址错.
 * 真实载体: builtin/pack-objects.c L1695
 *       `static struct pbase_tree_cache *(pbase_tree_cache[256]);`
 *       (配套 L1945 `free(pbase_tree_cache[i]->tree_data);`)
 * 修法: L17981 `隔排(空组, 0, 8, 缓四);` 之后补 `若 (首深 >= 0) 变量表[变量数 - 1].型序标 = 首深;`
 */

struct pbc {
    int tree_data;
    int oid;
};

/* ① 括号 指针数组 (本体) */
static struct pbc *(cache[4]);

/* ② 对照: 无括号 指针数组 (本来就正确) */
static struct pbc *plain[4];

static int 读括号(int i) { return cache[i]->tree_data; }   /* 声明 在 定义 之前使用 */
static int 写括号(int i, int v) { struct pbc *e = cache[i]; e->tree_data = v; return e->tree_data; }
static int 读无括号(int i) { return plain[i]->tree_data; }

static struct pbc g1, g2, g3, g4;

int main(void) {
    int i;
    cache[0] = &g1; cache[1] = &g2; cache[2] = &g3; cache[3] = &g4;
    for (i = 0; i < 4; i++) { cache[i]->tree_data = 0; cache[i]->oid = 0; }

    /* 逐元素 独立寻址: 若 元素尺寸/类型 错, 会 互相 覆盖 或 读错 */
    for (i = 0; i < 4; i++) 写括号(i, 100 + i);
    for (i = 0; i < 4; i++) if (读括号(i) != 100 + i) return 1 + i;

    if (g4.tree_data != 103) return 9;   /* cache[3] == &g4 */

    plain[0] = &g1;
    if (读无括号(0) != 100) return 10;   /* g1 已被 写括号(0,100) 置 100 */

    printf("PASS\n");
    return 0;
}
