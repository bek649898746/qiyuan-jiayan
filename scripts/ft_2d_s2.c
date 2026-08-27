#include <stdio.h>

int main(void) {
    /* 场景2: 2 个 2 级数组 */
    int A[2][2] = { {1, 2}, {3, 4} };
    int B[2][2] = { {5, 6}, {7, 8} };
    printf("S2 A00=%d A11=%d B00=%d B11=%d (want 1 4 5 8)\n", A[0][0], A[1][1], B[0][0], B[1][1]);
    printf("DONE2\n");
    return 0;
}
