// @EXPECTED err:A9002
#include <stdio.h>
/* 显式I2 推导化收口 负例 (2026-09-07): 复合赋值 double→int 无转 → A9002 */
int main(void) {
    int x = 5;
    x += 3.14;   /* 复合赋值 无转 → A9002 */
    return x;
}
