#include <stdio.h>

int main(void) {
    int A[2][2] = { {1, 2}, {3, 4} };
    int B[2][2] = { {5, 6}, {7, 8} };
    int C[2][2] = { {-1, -1}, {-1, -1} };
    矩阵乘 C, A, B, 1, 1, 2;
    printf("C00=%d C01=%d (want 5 6; -1 = 循环没写)\n", C[0][0], C[0][1]);
    printf("DONE\n");
    return 0;
}
