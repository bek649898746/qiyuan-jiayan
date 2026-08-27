#include <stdio.h>

int main(void) {
    int A[2][2] = { {1, 2}, {3, 4} };
    printf("A00 before B=%d (want 1)\n", A[0][0]);
    int B[2][2] = { {5, 6}, {7, 8} };
    printf("A00 after B=%d (want 1)\n", A[0][0]);
    printf("B00=%d (want 5)\n", B[0][0]);
    printf("DONE\n");
    return 0;
}
