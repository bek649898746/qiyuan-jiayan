// @EXPECTED exit:0
#include <stdio.h>
/* 显式I2 (2026-09-07): 无隐式转换 传参方向 — 显式 转 / double 形参 放行 */
int 加一(int x) { return x + 1; }
int 加双(双 x) { return (int)(x + 0.5); }
int main(void) {
    int a = 加一(转 int(3.14));
    if (a != 4) return 1;
    int b = 加双(3.14);
    if (b != 3) return 2;
    printf("PASS\n");
    return 0;
}
