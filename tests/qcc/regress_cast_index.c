// @EXPECTED exit:0
// 回归: 取地址/强转基址索引 (fix 2026-08-09)
// 根因: case-14 的 else 分支只认 pointer-param, 基址是 node-11(取地址) 时
//       load_param_val(空名) → 垃圾指针 0xC0000005。读写路径都缺通用基址分支。
#include <stdio.h>
int main(void) {
    char buf[8];
    int arr[4];
    int i;
    for (i = 0; i < 8; i++) buf[i] = (char)(10 + i);
    for (i = 0; i < 4; i++) arr[i] = 100 + i;

    /* 读: 取地址基址 + 索引 */
    if ((&buf[0])[2] != 12) return 1;
    if (((char*)&buf[0])[2] != 12) return 2;
    if (((char*)&buf[1])[3] != 14) return 3;
    if ((&arr[0])[2] != 102) return 4;

    /* 写: 取地址基址 + 索引 */
    (&buf[0])[2] = 55;
    if (buf[2] != 55) return 5;
    ((char*)&buf[3])[1] = 66;
    if (buf[4] != 66) return 6;

    printf("castidx-ok\n");
    return 0;
}
