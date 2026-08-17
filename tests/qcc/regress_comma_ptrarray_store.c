// @EXPECTED exit:0
// regress: 逗号声明指针数组 (int *rva[4], *rsym[4], *rtyp[4]) 双重索引 store 修复
// @EXPECTED out:59-132-178-238-284-330-390-514
// 最小复现 C: rva + rsym + rtyp 三数组 store (b2 用 3 次)
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int sites[8] = {59,132,178,238,284,330,390,514};
    int *rva[4], *rsym[4], *rtyp[4];
    int ridx[4] = {0,0,0,0};
    for (int i = 0; i < 4; i++) { rva[i] = calloc(8, 4); rsym[i] = calloc(8, 4); rtyp[i] = calloc(8, 4); }
    for (int i = 0; i < 8; i++) {
        int rsec = 0;
        int b2 = ridx[rsec]++;
        rva[rsec][b2] = sites[i];
        rsym[rsec][b2] = 1;
        rtyp[rsec][b2] = 4;
    }
    printf("%d-%d-%d-%d-%d-%d-%d-%d\n", rva[0][0], rva[0][1], rva[0][2], rva[0][3],
           rva[0][4], rva[0][5], rva[0][6], rva[0][7]);
    for (int i = 0; i < 4; i++) { free(rva[i]); free(rsym[i]); free(rtyp[i]); }
    return 0;
}
