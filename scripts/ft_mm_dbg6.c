#include <stdio.h>

int main(void) {
    int A[2][2] = { {1, 2}, {3, 4} };
    int B[2][2] = { {5, 6}, {7, 8} };
    int flat[4] = { 0 };
    /* 用展平读 A (2 级下标机制) */
    展平 flat, A, 2, 2;
    printf("flatA: %d %d %d %d (want 1 2 3 4)\n", flat[0], flat[1], flat[2], flat[3]);
    /* 用展平读 B */
    展平 flat, B, 2, 2;
    printf("flatB: %d %d %d %d (want 5 6 7 8)\n", flat[0], flat[1], flat[2], flat[3]);
    /* 用户 C 直接读 */
    printf("A: %d %d %d %d (want 1 2 3 4)\n", A[0][0], A[0][1], A[1][0], A[1][1]);
    printf("DONE\n");
    return 0;
}
