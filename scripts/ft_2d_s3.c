#include <stdio.h>

int main(void) {
    /* 场景3: 3 个 2 级数组 */
    int A[2][2] = { {1, 2}, {3, 4} };
    int B[2][2] = { {5, 6}, {7, 8} };
    int C[2][2] = { {9, 10}, {11, 12} };
    printf("S3 A00=%d A11=%d B00=%d B11=%d C00=%d C11=%d (want 1 4 5 8 9 12)\n", A[0][0], A[1][1], B[0][0], B[1][1], C[0][0], C[1][1]);
    printf("DONE3\n");
    return 0;
}
