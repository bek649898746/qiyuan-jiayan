#include <stdio.h>
/* 显式I2 负例 (2026-09-07): 整型函数返回 双精度 无显式转 → A9002 */
int 加(void) { return 3.14; }
int main(void) { return 加(); }
