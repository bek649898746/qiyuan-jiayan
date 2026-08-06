// @EXPECTED exit:0
// @EXPECTED out:PASS
#include <stdio.h>
/* 回归: unsigned 六种比较 + 混合符号 (M1 fix) */
int main(void) {
    unsigned int a = 0xFFFFFFFFu, b = 1u;
    if (!(a > b)) return 1;
    if (a < b) return 2;
    if (!(a >= b)) return 3;
    if (a <= b) return 4;
    if (!(a != b)) return 5;
    if (a == b) return 6;
    /* 混合符号: C 标准语义 = int 转 unsigned 再比 (gcc 验证一致) */
    int c = -1;              /* (unsigned)-1 == 0xFFFFFFFF == a */
    if (a > c) return 7;     /* 0xFFFFFFFFu > 0xFFFFFFFFu == false */
    if (a != (unsigned)c) return 8;
    if (!(a > 1)) return 9;  /* 0xFFFFFFFFu > 1u == true */
    if (a <= 1) return 10;
    printf("PASS\n");
    return 0;
}
