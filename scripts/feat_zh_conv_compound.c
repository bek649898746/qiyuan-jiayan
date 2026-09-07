// @EXPECTED exit:0
#include <stdio.h>
/* 显式I2 推导化收口 (2026-09-07): 复合赋值/三元/数组初始化 显式转/double数组 放行 */
int main(void) {
    int x = 5;
    x += 转 int(3.14);
    int y = 1 ? 转 int(3.14) : 5;
    int a[2] = { 转 int(3.14), 5 };
    双 b[2] = { 3.14, 5.0 };
    if (x != 8 || y != 3 || a[0] != 3 || b[0] < 3.0) return 1;
    printf("PASS\n");
    return 0;
}
