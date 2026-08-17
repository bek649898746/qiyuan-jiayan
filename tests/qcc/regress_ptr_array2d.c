// @EXPECTED exit:0
// @EXPECTED out:1
// regress: int *arr[4] 指针数组二维赋值 (write_coff_obj rtyp[rsec][b2])
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int *rtyp[4];
    for (int i = 0; i < 4; i++)
        rtyp[i] = malloc(4 * sizeof(int));
    rtyp[0][0] = 4;
    rtyp[0][1] = 2;
    rtyp[1][3] = 4;
    rtyp[2][2] = 4;
    rtyp[3][0] = 4;
    printf("%d\n", rtyp[0][0] + rtyp[1][3] + rtyp[3][0]);
    for (int i = 0; i < 4; i++) free(rtyp[i]);
    return 0;
}
