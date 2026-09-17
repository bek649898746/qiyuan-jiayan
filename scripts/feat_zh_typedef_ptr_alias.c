// @EXPECTED out:PASS
#include <stdio.h>

/* C54 补锚 — typedef 指针别名 丢类型 + 同名遮蔽 (第 54 站, 2026-09-17)
 *
 * 根因: `typedef struct S *SP;` 注册时 只记 尺寸 不记「是 指针」
 *       ⟹ 声明 `SP p;` 按 尺寸==8 走 **8 字节整数登记入口** (余乙)
 *       ⟹ 该 入口 的 复用分支 **只按 名字 匹配** ⟹ 与 函数形参 同名 时
 *          静默 复用 形参 条目 ⟹ `p->a` 按 **形参 类型** 查 字段 ⟹ A9001。
 * 触发 载体 (真实): compat/nedmalloc/nedmalloc.c threadcache_free
 *       (`mchunkptr p = mem2chunk(mem);` 遮蔽 形参 `nedpool *p`)。
 * 本 锚 覆盖: ① 同名 遮蔽 + 块内 使用 (原 误报 面) ② 改名 对照 ③ 出块 恢复。
 */

typedef struct S *SP;
struct S { int a; };
typedef struct T *TP;
struct T { int b; };

/* ① 同名遮蔽: 块内 SP p 遮蔽 形参 p —— 块内 p->a 必须 按 SP (= 构 S *) 解析 */
static int 遮蔽(TP p) {
    int 和 = p->b;
    {
        SP p = 0;
        if (p != 0) 和 += p->a;
    }
    return 和;
}

/* ② 改名对照: 同一形态但块内 换 名字 (原 也 通过, 防 修复 引入 新 回归) */
static int 改名(TP p) {
    int 和 = p->b;
    {
        SP q = 0;
        if (q != 0) 和 += q->a;
    }
    return 和;
}

/* ③ 出块恢复: 块内 遮蔽 不得 污染 块外 的 p */
static int 出块恢复(TP p) {
    int 和 = p->b;
    {
        SP p = 0;
        if (p != 0) 和 += p->a;
    }
    return 和 + p->b;
}

int main(void) {
    struct T 甲;
    甲.b = 21;

    if (遮蔽(&甲) != 21) return 1;
    if (改名(&甲) != 21) return 2;
    if (出块恢复(&甲) != 42) return 3;

    printf("PASS\n");
    return 0;
}
