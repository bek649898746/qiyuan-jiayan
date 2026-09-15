// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 合拼 (合拼 dst, a, na, b, nb) 接入测试 (2026-08-27):
 *   合拼 dst, a, na, b, nb;  →  顺序复制两段 (a 后接 b)
 *   ⚠️ 词改自"拼接" — 拼接已被 lexer 映射 strcat (字符串库映射), 冲突
 */

int main(void) {
    /* ① 基本合拼 */
    int a[3] = { 1, 2, 3 };
    int b[3] = { 4, 5, 6 };
    int dst[6];
    合拼 dst, a, 3, b, 3;    /* [1,2,3,4,5,6] */
    if (dst[0] != 1 || dst[2] != 3 || dst[3] != 4 || dst[5] != 6) return 1;

    /* ② 不等长 */
    int a2[2] = { 10, 20 };
    int b2[4] = { 30, 40, 50, 60 };
    int dst2[6];
    合拼 dst2, a2, 2, b2, 4;  /* [10,20,30,40,50,60] */
    if (dst2[0] != 10 || dst2[1] != 20 || dst2[2] != 30 || dst2[5] != 60) return 2;

    /* ③ b 空 */
    int dst3[2];
    合拼 dst3, a2, 2, b2, 0;  /* [10,20] */
    if (dst3[0] != 10 || dst3[1] != 20) return 3;

    /* ④ 与排序组合 */
    int x[2] = { 3, 1 };
    int y[3] = { 5, 2, 4 };
    排序 x, 2;                 /* [1,3] */
    排序 y, 3;                 /* [2,4,5] */
    int dst4[5];
    合拼 dst4, x, 2, y, 3;    /* [1,3,2,4,5] */
    if (dst4[0] != 1 || dst4[1] != 3 || dst4[2] != 2 || dst4[4] != 5) return 4;

    printf("PASS\n");
    return 0;
}
