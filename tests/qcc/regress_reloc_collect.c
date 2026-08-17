// @EXPECTED exit:0
// @EXPECTED out:4-4-4-0-1-59
// regress: write_coff_obj reloc 收集模式 (crel struct + rtyp 指针数组)
#include <stdio.h>
#include <stdlib.h>

struct crel_t { int site; int type; int sym; int addend; int is_label; int label; };
static struct crel_t crel[256];
static int crel_n;

int main(void) {
    // 模拟 coff_crel 调用
    crel[crel_n].site = 59; crel[crel_n].type = 4; crel[crel_n].sym = 0; crel[crel_n].addend = 0; crel_n++;
    crel[crel_n].site = 132; crel[crel_n].type = 4; crel[crel_n].sym = 1; crel[crel_n].addend = 0; crel_n++;
    crel[crel_n].site = 178; crel[crel_n].type = 4; crel[crel_n].sym = 1; crel[crel_n].addend = 0; crel_n++;
    // 模拟 rva/rsym/rtyp 收集
    int nrel[4] = {0,0,0,0};
    int cp = 1000;
    for (int i = 0; i < crel_n; i++) {
        int site = crel[i].site;
        int rsec = 0;
        if (site >= 2000000000) rsec = 3;
        else if (site >= cp) rsec = 1;
        nrel[rsec]++;
    }
    int *rva[4], *rsym[4], *rtyp[4];
    int ridx[4] = {0,0,0,0};
    for (int i = 0; i < 4; i++) {
        rva[i] = calloc(nrel[i] ? nrel[i] : 1, 4);
        rsym[i] = calloc(nrel[i] ? nrel[i] : 1, 4);
        rtyp[i] = calloc(nrel[i] ? nrel[i] : 1, 4);
    }
    for (int i = 0; i < crel_n; i++) {
        int site = crel[i].site;
        int rsec = 0;
        if (site >= 2000000000) rsec = 3;
        else if (site >= cp) rsec = 1;
        int b2 = ridx[rsec]++;
        rva[rsec][b2] = site;
        rsym[rsec][b2] = crel[i].sym;
        rtyp[rsec][b2] = crel[i].type;
    }
    // 验证
    printf("%d-%d-%d-%d-%d-%d\n", rtyp[0][0], rtyp[0][1], rtyp[0][2], rsym[0][0], rsym[0][1], rva[0][0]);
    for (int i = 0; i < 4; i++) { free(rva[i]); free(rsym[i]); free(rtyp[i]); }
    return 0;
}
