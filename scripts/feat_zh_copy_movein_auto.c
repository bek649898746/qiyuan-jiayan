// @EXPECTED exit:0
#include <stdio.h>
/* t8b (2026-09-07): 移入 auto-Copy 构 (全标量字段) → 源不失效
   (复制查 显式-only → 类型可复制 权威 修复; 移入 与 赋值 对齐) */
构 点 { 整 x; 整 y; };   /* 全标量 → auto-Copy */

函数 整 主(空) {
    构 点 a; a.x = 3; a.y = 4;
    构 点 b; b.x = 0; b.y = 0;
    移入 b, a;            /* a 是 auto-Copy → 源不失效 */
    若 (b.x != 3 || b.y != 4) { 格式化 "FAIL b: %d %d\n", b.x, b.y; 返 1; }
    若 (a.x != 3 || a.y != 4) { 格式化 "FAIL a: %d %d\n", a.x, a.y; 返 2; }  /* 修复后 a 仍可用 */
    格式化 "PASS\n";
    返 0;
}
