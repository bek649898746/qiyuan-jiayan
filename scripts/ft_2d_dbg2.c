#include <stdio.h>

int main(void) {
    int A[2][2] = { {1, 2}, {3, 4} };
    printf("A00=%d A01=%d A10=%d A11=%d (want 1 2 3 4)\n", A[0][0], A[0][1], A[1][0], A[1][1]);
    /* 内存平铺查看: 用 1 级指针遍历 */
    int *p = (int *)A;
    printf("flat: %d %d %d %d (want 1 2 3 4)\n", p[0], p[1], p[2], p[3]);
    /* sizeof */
    printf("sizeofA=%d sizeofA0=%d (want 16 8)\n", (int)sizeof(A), (int)sizeof(A[0]));
    /* 行内下标 */
    int r0 = A[0][0];
    int r1 = A[0][1];
    printf("r0=%d r1=%d (want 1 2)\n", r0, r1);
    printf("DONE\n");
    return 0;
}
