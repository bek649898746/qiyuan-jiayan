#include <stdio.h>

int main(void) {
    /* 1 级数组初始化 */
    int a[4] = { 10, 20, 30, 40 };
    printf("a0=%d a3=%d (want 10 40)\n", a[0], a[3]);
    /* 2 级数组初始化 */
    int A[2][2] = { {1, 2}, {3, 4} };
    printf("A00=%d A11=%d (want 1 4)\n", A[0][0], A[1][1]);
    /* 2 级手动赋值 */
    A[0][0] = 100;
    printf("A00set=%d (want 100)\n", A[0][0]);
    /* 2 级读+写 */
    int B[2][2] = { {0, 0}, {0, 0} };
    B[0][1] = A[0][1];
    printf("B01=%d (want 2)\n", B[0][1]);
    printf("DONE\n");
    return 0;
}
