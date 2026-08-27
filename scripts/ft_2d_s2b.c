#include <stdio.h>

int main(void) {
    int A[2][2] = { {1, 2}, {3, 4} };
    int B[2][2] = { {5, 6}, {7, 8} };
    unsigned char *ma = (unsigned char *)A;
    unsigned char *mb = (unsigned char *)B;
    printf("&A=%p &B=%p (diff=%d)\n", (void*)A, (void*)B, (int)((char*)B - (char*)A));
    printf("A bytes:");
    for (int i = 0; i < 32; i++) { printf(" %02x", ma[i]); if (i % 8 == 7) printf(" |"); }
    printf("\n");
    printf("B bytes:");
    for (int i = 0; i < 32; i++) { printf(" %02x", mb[i]); if (i % 8 == 7) printf(" |"); }
    printf("\n");
    printf("A00=%d A01=%d A10=%d A11=%d\n", A[0][0], A[0][1], A[1][0], A[1][1]);
    printf("B00=%d B01=%d B10=%d B11=%d\n", B[0][0], B[0][1], B[1][0], B[1][1]);
    printf("DONE\n");
    return 0;
}
