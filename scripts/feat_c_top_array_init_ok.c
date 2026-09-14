/* feat_c_top_array_init_ok.c — 顶层 数组 变量 花括 初始化器 (A75b/c/d/e/f 根治 2026-09-14)
   病灶 (四条 路径 缺陷, 一次 收口):
     ① 顶层 内联 结构体 数组 (具名 L16775): 原 只 数 元素、**从不 生成 初始化** ⟹ 定长 [N] 静默 全 0;
        空 [] 因 计数二 仍 = 1 误进 值布 (字段级) ⟹ 嵌套 花括 被 当 opaque ⟹ 域序 溢出 ⟹ 吞后续 (中断)。
     ② 匿名 结构体 顶层数组 (L17688): 只 空维度 走 左升, 定长 落 值布 ⟹ 静默 全 0。
     ③ 左升 的 `[序标].字段 = 值` 无分支 ⟹ 词位 停 在 `.` ⟹ 落叶 赋值级() 不推进 ⟹ **死循环 (AST 溢出)**。
     ④ 维度 前瞻循环 消费 维度值 后 未 推进 ⟹ **多维 只 乘 首维** (sizeof(arr) 16 应 32) ⟹ 存储不足 ⟹ 越界。
   修法: 两 路径 统一 走 既有 左升 (元素级 生成器) + 传 真 维表 + 补 每维 行宽 (域行数) + 左升 接 `[序标].字段`。
   本锚: 定长 / 空维度 / 二维 / 设计器 ([序标] 与 [序标].成员) / 匿名结构体 / 标量数组 / sizeof。
   说明: 旧种子 (89476ACC) 上本锚 多处 失败 (中断 / 静默 0); 修复后 与 gcc 逐字节一致。 */
// @EXPECTED out:f=1234
// @EXPECTED out:e=5678
// @EXPECTED out:2d=12345678
// @EXPECTED out:d=10000900
// @EXPECTED out:a=1234
// @EXPECTED out:i=0070
// @EXPECTED out:sz=16,8
// @EXPECTED exit:0
#include <stdio.h>

struct q { int a, b; } af[2] = { {1,2}, {3,4} };
struct q ae[] = { {5,6}, {7,8} };
struct q a2[2][2] = { { {1,2},{3,4} }, { {5,6},{7,8} } };
struct q ad[4] = { [2].b = 9, [0].a = 1 };
struct q aa[] = { {1,2}, {3,4} };                             /* 具名 空维度 (匿名形态 另见 feat_c_anon_array_init_ok.c) */
int ai[4] = { [2] = 7 };

int main(void)
{
    printf("f=%d%d%d%d\n", af[0].a, af[0].b, af[1].a, af[1].b);
    printf("e=%d%d%d%d\n", ae[0].a, ae[0].b, ae[1].a, ae[1].b);
    printf("2d=%d%d%d%d%d%d%d%d\n", a2[0][0].a, a2[0][0].b, a2[0][1].a, a2[0][1].b, a2[1][0].a, a2[1][0].b, a2[1][1].a, a2[1][1].b);
    printf("d=%d%d%d%d%d%d%d%d\n", ad[0].a, ad[0].b, ad[1].a, ad[1].b, ad[2].a, ad[2].b, ad[3].a, ad[3].b);
    printf("a=%d%d%d%d\n", aa[0].a, aa[0].b, aa[1].a, aa[1].b);
    printf("i=%d%d%d%d\n", ai[0], ai[1], ai[2], ai[3]);
    printf("sz=%d,%d\n", (int)sizeof(af), (int)sizeof(af[0]));
    return 0;
}
