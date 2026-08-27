#include <stdio.h>

int main(void) {
    判别 A 于 { X, Y, Z };
    printf("A_X=%d A_Y=%d A_Z=%d (want 0 1 2)\n", A_X, A_Y, A_Z);
    /* 判别带负载 */
    判别 B 于 { M(v), N(w), P() };
    printf("B_M=%d B_N=%d B_P=%d (want 0 1 2)\n", B_M, B_N, B_P);
    printf("DONE\n");
    return 0;
}
