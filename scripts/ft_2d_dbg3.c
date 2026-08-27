#include <stdio.h>

int main(void) {
    int A[2][2] = { {1, 2}, {3, 4} };
    int *p = (int *)A;
    printf("&A=%p A=%p &A00=%p &A01=%p &A10=%p &A11=%p\n", (void*)&A, (void*)A, (void*)&A[0][0], (void*)&A[0][1], (void*)&A[1][0], (void*)&A[1][1]);
    printf("p=%p &p1=%p &p2=%p &p3=%p\n", (void*)p, (void*)&p[1], (void*)&p[2], (void*)&p[3]);
    printf("A00=%d A01=%d A10=%d A11=%d\n", A[0][0], A[0][1], A[1][0], A[1][1]);
    printf("p0=%d p1=%d p2=%d p3=%d\n", p[0], p[1], p[2], p[3]);
    /* 步长 */
    printf("A1-A0=%d (want 8) A01-A00=%d (want 4)\n", (int)((char*)A[1] - (char*)A[0]), (int)((char*)&A[0][1] - (char*)&A[0][0]));
    printf("DONE\n");
    return 0;
}
