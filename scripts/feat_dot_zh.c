// @EXPECTED out:PASS
#include <stdio.h>

/* 甲言 点积 接入测试 (2026-08-28): 点积 d, a, b, n; — 向量内积 Σa[i]*b[i] (蓝图 §一 矩阵/向量族) */

int main(void) {
    int a[3] = { 1, 2, 3 };
    int b[3] = { 4, 5, 6 };
    int d = 0;
    点积 d, a, b, 3;   /* 1*4 + 2*5 + 3*6 = 32 */
    if (d != 32) { printf("FAIL dot1: %d\n", d); return 1; }
    /* 零向量 */
    int z[2] = { 0, 0 };
    int w[2] = { 1, 2 };
    点积 d, z, w, 2;   /* 0 */
    if (d != 0) { printf("FAIL dot2: %d\n", d); return 1; }
    /* 负数 */
    int m[2] = { -1, 2 };
    int n2[2] = { 3, -4 };
    点积 d, m, n2, 2;  /* -3 + -8 = -11 */
    if (d != -11) { printf("FAIL dot3: %d\n", d); return 1; }
    /* 变量 n */
    int nv = 1;
    点积 d, a, b, nv;  /* 只算 1*4 = 4 */
    if (d != 4) { printf("FAIL dot4: %d\n", d); return 1; }
    /* 数组间无干扰: a 不变 */
    if (a[0] != 1 || a[1] != 2 || a[2] != 3) { printf("FAIL dot5 a\n"); return 1; }
    if (b[0] != 4 || b[1] != 5 || b[2] != 6) { printf("FAIL dot5 b\n"); return 1; }
    /* 重复调用: 累加器独立 */
    点积 d, a, b, 3;
    点积 d, a, b, 3;
    if (d != 32) { printf("FAIL dot6: %d\n", d); return 1; }
    printf("PASS\n");
    return 0;
}
