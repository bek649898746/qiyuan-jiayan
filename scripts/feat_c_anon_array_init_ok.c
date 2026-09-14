/* feat_c_anon_array_init_ok.c — 匿名 结构体 数组 顶层 花括 初始化 (A75b 根治 2026-09-14)
   病灶: `结构体 { ... } 变量[] = { ... };` (匿名 结构体 + 顶层数组) —— 匿名路径 (L17688) 原
         **只 空维度 走 左升**, 定长 落 值布 (字段级) ⟹ 嵌套花括被当 opaque ⟹ 域序 溢出 ⟹ 吞后续 (中断);
         且 空维度 时 原 代码 只 数 元素、不生成初始化器。
   修法: 匿名路径 同样 统一走 左升 (元素级 生成器) + 传 真 维表 + 补 每维 行宽。
   本锚: 单个 匿名 结构体 空维度数组 (与 具名 锚 feat_c_top_array_init_ok.c 互补)。
   ⚠️ 已知 既存 缺陷 (另记, 非本锚 覆盖): **两个 匿名 数组 相邻** (或 匿名 数组 在 其它 数组 声明 之后)
      元素数推断错位 (得 3 应 2 → A2001 假越界) — HEAD 同样失败, 本锚 刻意 只 放 一个 匿名数组。 */
// @EXPECTED out:e=55
// @EXPECTED out:sz=16,8
// @EXPECTED exit:0
#include <stdio.h>

struct { int a, b; } ae[] = { {1,2}, {3,4} };

int main(void)
{
    printf("e=%d%d\n", ae[0].a + ae[1].b, ae[1].a + ae[0].b);
    printf("sz=%d,%d\n", (int)sizeof(ae), (int)sizeof(ae[0]));
    return 0;
}
