// @EXPECTED exit:0
// @EXPECTED out:0-0-0-1-0
// 回归: char 数组元素 ++/-- 必须是 8 位 RMW (fix 2026-08-17)
// 原 case 23/26 固定 32 位读写 → arr[119]++ 写 4 字节污染 arr[118..121] → 误读
// (git parse-options 'short name already used' 根因)
#include <stdio.h>

int main(void) {
    char arr[128];
    int i;
    for (i = 0; i < 128; i++) arr[i] = 0;
    int r_w = arr[119]++;   /* 先 119: 若 32 位 RMW 会污染 [118..121] */
    int r_v = arr[118]++;   /* 再 118: 4 字节读会捡到 119 的字节 → 旧值 256 */
    int r_g = arr[103]++;   /* 105 的 4 字节写在 [105..108], 103 的 4 字节读 [103..106] */
    arr[105]++;
    int r_d = arr[103]++;   /* 若 32 位 RMW, [103..106] 含 105 的字节 → 旧值 256 */
    arr[118]--;             /* 前缀/后缀递减同样 8 位 */
    printf("%d-%d-%d-%d-%d\n", r_w, r_v, r_g, r_d, arr[118]);
    return 0;
}
