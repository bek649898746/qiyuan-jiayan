// @EXPECTED exit:0
#include <stdio.h>
/* 显式I2 (2026-09-07): 无隐式转换 返回方向 — double 函数返 double 放行, 显式 转 放行 */
双 加双() { return 3.14; }
int 加整() { return 转 int(3.14); }
int main(void) {
    双 d = 加双();
    if (d < 3.0 || d > 3.2) return 1;
    int x = 加整();
    if (x != 3) return 2;
    printf("PASS\n");
    return 0;
}
