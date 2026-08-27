#include <stdio.h>

int main(void) {
    int A[2][2] = { {1, 2}, {3, 4} };
    int B[2][2] = { {5, 6}, {7, 8} };
    int C[2][2] = { {0, 0}, {0, 0} };
    /* 2 级下标读取 (普通 C 表达式) */
    int t = A[0][0] + A[1][1] + B[0][1] + B[1][0];
    printf("t=%d (want 1+4+6+7=18)\n", t);
    /* 2 级下标乘法 */
    int m = A[0][1] * B[1][0];
    printf("m=%d (want 2*7=14)\n", m);
    /* 嵌套 for 手写矩阵乘 (纯 C) */
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++) {
            int acc = 0;
            for (int k = 0; k < 2; k++) acc += A[i][k] * B[k][j];
            C[i][j] = acc;
        }
    printf("C=%d %d %d %d (want 19 22 43 50)\n", C[0][0], C[0][1], C[1][0], C[1][1]);
    printf("DONE\n");
    return 0;
}
