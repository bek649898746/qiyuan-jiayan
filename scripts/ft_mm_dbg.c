#include <stdio.h>

int main(void) {
    int A[1][1] = { {3} };
    int B[1][1] = { {7} };
    int C[1][1] = { {0} };
    矩阵乘 C, A, B, 1, 1, 1;
    printf("C00=%d (want 21)\n", C[0][0]);
    /* 2x2 */
    int A2[2][2] = { {1, 2}, {3, 4} };
    int B2[2][2] = { {5, 6}, {7, 8} };
    int C2[2][2] = { {0, 0}, {0, 0} };
    矩阵乘 C2, A2, B2, 2, 2, 2;
    printf("C2=%d %d %d %d (want 19 22 43 50)\n", C2[0][0], C2[0][1], C2[1][0], C2[1][1]);
    printf("DONE\n");
    return 0;
}
