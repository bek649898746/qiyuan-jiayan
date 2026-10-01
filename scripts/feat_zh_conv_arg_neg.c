// @EXPECTED err:A9002
#include <stdio.h>
/* 显式I2 负例 (2026-09-07): double 实参 传 int 形参 无显式转 → A9002 */
int 加一(int x) { return x + 1; }
int main(void) {
    双 d = 3.14;
    return 加一(d);
}
