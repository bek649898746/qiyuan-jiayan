// @EXPECTED exit:0
// @EXPECTED out:PASS
#include <stdio.h>
/* 回归: 多维数组 声明/初始化/访问/写入 (brace_arr_init + gi_idx 多维游标) */
int g[2][3] = { {1,2,3}, {4,5,6} };

int main(void) {
    /* 嵌套行初始化 */
    int a[3][4] = {{1,2,3,4},{5,6,7,8},{9,10,11,12}};
    if (a[1][2] != 7)  { printf("FAIL a[1][2]=%d\n", a[1][2]); return 1; }
    if (a[2][3] != 12) { printf("FAIL a[2][3]=%d\n", a[2][3]); return 2; }
    if (a[0][0] != 1)  { printf("FAIL a[0][0]=%d\n", a[0][0]); return 3; }
    /* 扁平初始化 (行连续填充) */
    int b[2][3] = {1,2,3,4,5,6};
    if (b[1][2] != 6)  { printf("FAIL b[1][2]=%d\n", b[1][2]); return 4; }
    if (b[1][0] != 4)  { printf("FAIL b[1][0]=%d\n", b[1][0]); return 5; }
    /* 全局多维数组 */
    if (g[1][0] != 4)  { printf("FAIL g[1][0]=%d\n", g[1][0]); return 6; }
    if (g[0][2] != 3)  { printf("FAIL g[0][2]=%d\n", g[0][2]); return 7; }
    /* 元素写入 */
    a[2][1] = 99;
    if (a[2][1] != 99) { printf("FAIL write=%d\n", a[2][1]); return 8; }
    /* 三维 */
    int c[2][2][2] = {{{1,2},{3,4}},{{5,6},{7,8}}};
    if (c[1][1][1] != 8) { printf("FAIL c[1][1][1]=%d\n", c[1][1][1]); return 9; }
    if (c[0][1][0] != 3) { printf("FAIL c[0][1][0]=%d\n", c[0][1][0]); return 10; }
    printf("PASS\n");
    return 0;
}
