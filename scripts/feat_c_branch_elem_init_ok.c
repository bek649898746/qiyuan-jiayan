/* feat_c_branch_elem_init_ok.c — 分支/循环 内 数组元素 互斥写 —— 元素级读检查 的 分支/回边 时机 (A103 + A104 + A104b 根治 2026-09-14)
   病灶: `#78 明确初始化` 的元素级读检查 (`尾阵查触(名, 位)`) 原在 **`赋值()` 语句末尾** 判定 ——
         而 then 分支 的 `t[1] = t[0];` 判定时, **else 分支 尚未解析** ⇒ `init_td(&t[i])` 里
         `&t[i]` 的 整体已触 (`尾阵触(t,-1)`) 未登记 ⇒ 误 A4001 (实测 git `merge-ort.c`
         `for (i = MERGE_BASE; i <= MERGE_SIDE2; i++) { if (i == 1 && side1_matches_mbase) t[1] = t[0];
         else if (i == 2 && sides_match) t[2] = t[1]; else {...} }`).
         **最小复现 r2 (无循环, 纯 if/else) 同样误报** ⇒ 推翻了台账里「必须循环回边」的判断 ——
         第一步其实是 **分支汇合时机**, 第二步才是 嵌套 else-if 链 与 循环回边.
   修法 (三层):
     A103 分支内 (`支层 > 0`) 的元素读 → 记入 本层 缓冲, 由 `支阵合()` 在 **汇合点** 判定 (此时 else 已解析);
     A104b 嵌套 `if / else if / else` 链 —— 写在外层支 / 读在内层支时, 内层汇合尚不见外层写 ⇒
       **未触读 上抛到外层缓冲**, 直到最外层 (`层 == 0`) 再判; **循环体内** (`环体深 > 0`) 放行 (回边语义);
     A104 循环体**内** 的 if-else 汇合 用 宽松规则 `P | T | E` (多轮执行 ⇒ 各支都可能执行过).
   本锚: 循环 + if/else 互斥写 → 跨支读 (结构体数组, 常数下标). */
// @EXPECTED out:7 7 9
// @EXPECTED exit:0
#include <stdio.h>

struct td { int x; };

static void init_td(struct td *d, int v)
{
    d->x = v;
}

int main(void)
{
    struct td t[3];
    int i;

    for (i = 0; i <= 2; i++) {
        if (i == 1)
            t[1] = t[0];
        else
            init_td(&t[i], i == 0 ? 7 : 9);
    }
    printf("%d %d %d\n", t[0].x, t[1].x, t[2].x);
    return 0;
}
