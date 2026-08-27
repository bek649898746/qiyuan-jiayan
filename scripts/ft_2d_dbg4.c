#include <stdio.h>

int main(void) {
    int A[2][2] = { {1, 2}, {3, 4} };
    unsigned char *m = (unsigned char *)A;
    printf("bytes:");
    for (int i = 0; i < 32; i++) { printf(" %02x", m[i]); if (i % 8 == 7) printf(" |"); }
    printf("\n");
    printf("A00=%d A01=%d A10=%d A11=%d\n", A[0][0], A[0][1], A[1][0], A[1][1]);
    printf("DONE\n");
    return 0;
}
