// @KNOWN-GAP: A9002 未触发 —— 整型函数 `return 3.14;` 应报「双精度 转 整型 无显式转换」, 实测编译成功 (2026-09-26 round 29 审计发现)
#include <stdio.h>
/* 显式I2 负例 (2026-09-07): 整型函数返回 双精度 无显式转 → A9002 */
int 加(void) { return 3.14; }
int main(void) { return 加(); }
