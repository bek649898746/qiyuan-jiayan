// @EXPECTED exit:nonzero
#include <stdio.h>
/* 显式F2 负例 (2026-09-07): 运行时除零 → __qcc_div_check 优雅 exit(1) */
int main(void) {
    int a = 10, b = 0;
    int c = a / b;
    printf("FAIL: %d\n", c);
    return 0;
}
