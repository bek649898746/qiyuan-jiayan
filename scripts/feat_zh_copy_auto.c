# 包含 <stdio.h>
/* #16 Copy 自动派生 (2026-09-05 S7): 全标量字段构 → 自动 Copy (赋值=复制, 源可用);
   含指针构 非 Copy 由 feat_zh_copy_auto_neg 覆盖 */
构 点 { 整 x; 整 y; }            /* 全标量 → 自动 Copy */

函数 整 主(空) {
    构 点 a; a.x = 3; a.y = 4;
    构 点 b; b = a;                /* 自动 Copy → a 仍可用 */
    若 (a.x != 3 || b.y != 4) { 格式化 "FAIL 点: %d %d\n", a.x, b.y; 返 1; }
    格式化 "Copy 自动派生 PASS\n", 0;
    返 0;
}
