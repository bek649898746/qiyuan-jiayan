/* feat_c_addr_taken_array_ok.c — 取地址 写 数组 = 元素级 已初始化 (A76 根治 2026-09-14)
   病灶: `#78 明确初始化` 的 元素级追踪 有 **两套登记** —— 标量 `就绪丙` / 数组 `就绪阵丙`,
         而 `尾逃逸` (&expr 取地址 = 潜在写) **只查 标量那套** ⟹ 数组变量 命中 `尾索 < 0` 早退
         ⟹ `&arr[i]` 的潜在写 被忽略 ⟹ 后续 读 `arr[i]` 误 A4001.
   最小复现 (r3): `init_tree_desc(&t[0]); t[1] = t[0]; return t[1].x;` → 甲言 `A4001 数组元素 't[0]' 未初始化
         就读取`; gcc 正常. 旧种子 同样失败 (既存).
   修法: `尾逃逸` 里 数组 (尾阵索 >= 0) 改走 `尾阵触(名, -1)` —— 「动态下标 降 不追踪」,
         整个数组 视为 已触 (保守豁免). 标量路径 不变.
   本锚: `&arr[常数]` 出参初始化 → 后续 读/拷贝. */
// @EXPECTED out:14
// @EXPECTED out:77
// @EXPECTED exit:0
#include <stdio.h>

struct tree_desc { int x; };

static void init_tree_desc(struct tree_desc *d, int v) { d->x = v; }

int main(void)
{
    struct tree_desc t[3];
    init_tree_desc(&t[0], 7);
    t[1] = t[0];
    printf("%d\n", t[0].x + t[1].x);
    init_tree_desc(&t[2], 70);
    printf("%d\n", t[2].x + t[1].x);
    return 0;
}
