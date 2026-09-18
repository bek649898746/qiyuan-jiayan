// @EXPECTED out:ANONGLB PASS
/* 锚: 全局匿名结构体 + 内联具名体 — 「R2 嵌套体」家族 第 5 形态 (C99 第 99 站, 2026-09-18)
   形态: `struct { struct NK7 { V key_val; int n; } u; int j; } g7;` —— 外层 **匿名** 且 内层 是
         **内联具名体**; 对照的 前四形态 (见 feat_zh_nested_inline_struct_ok.c) 都在 **typedef/具名** 外层里。
   本锚 钉死 三处 同源缺陷 (缺一 必红):
     ① C100 L17841「结构词+类型名」分支 无条件 吞 `构 <名>` 只留 `{` ⟹ 外层字段循环条件
        `词型表[词位] != 右花` 被【内层】的 `}` 命中而退出 ⟹ 内层字段被当外层字段登记、
        外层余下字段与变量名 全部落空 ⟹ 全局变量 `g7` 从未登记 (objdump 无 `g7` 符号,
        `main` 无写 `g7.j` 的指令)。
     ② C101 匿名全局路径的内联具名体字段循环 缺 typedef 名 分支 ⟹ `V key_val` 的 `key_val`
        无类型 ⟹ `A9001 类型 'NK7' 的字段 'key_val' 不是 结构体/联合`。
     ③ C102 字段尺寸递归后 未调用 `字段类型` ⟹ 尺寸对 但 类型链 断, 仍报 A9001。
   锚灵敏度: 在 C99 之前的编译器上 本锚 **必红** (编译失败 或 静默 读回 0)。
   注: 不使用 `sizeof(g7)` —— 甲言 暂 不支持 `sizeof(变量)` (A1001), 判据 走 值 逐值 对照。 */
#include <stdio.h>
typedef union V { int i; } V;

struct { struct NK7 { V key_val; int n; } u; int j; } g7;   /* ① 本锚 主形态 */
struct { V top; int j; } g5;                                 /* 对照组: 平铺 typedef 字段 (本已绿) */
typedef struct NK6 { V key_val; int n; } NK6;
struct { NK6 u; int j; } g6;                                 /* 对照组: typedef 结构体字段 (本已绿) */

int main(void) {
    g7.u.key_val.i = 31; g7.u.n = 8; g7.j = 9;
    g5.top.i = 41; g5.j = 10;
    g6.u.key_val.i = 51; g6.u.n = 11; g6.j = 12;

    if (g7.u.key_val.i != 31 || g7.u.n != 8 || g7.j != 9) return 1;
    if (g5.top.i != 41 || g5.j != 10) return 2;
    if (g6.u.key_val.i != 51 || g6.u.n != 11 || g6.j != 12) return 3;
    printf("ANONGLB PASS\n");
    return 0;
}
