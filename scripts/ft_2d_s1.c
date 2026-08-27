#include <stdio.h>

int main(void) {
    /* 场景1: 只有 1 个 2 级数组 */
    int A[2][2] = { {1, 2}, {3, 4} };
    printf("S1 A00=%d A11=%d (want 1 4)\n", A[0][0], A[1][1]);
    printf("DONE1\n");
    return 0;
}
