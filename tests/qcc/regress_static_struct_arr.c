// @EXPECTED exit:0
// @EXPECTED out:4
// regress: static struct 数组字段读写 (write_coff_obj crel[].type)
#include <stdio.h>

struct crel_t { int site; int type; int sym; int addend; int is_label; int label; };
static struct crel_t crel[256];
static int crel_n;

static void add_crel(int site, int type, int sym, int addend) {
    crel[crel_n].site = site;
    crel[crel_n].type = type;
    crel[crel_n].sym = sym;
    crel[crel_n].addend = addend;
    crel_n++;
}

int main(void) {
    add_crel(59, 4, 0, 0);
    add_crel(132, 4, 1, 0);
    int t = crel[0].type;
    int t2 = crel[1].type;
    int s = crel[0].sym;
    printf("%d %d %d\n", t, t2, s);
    return 0;
}
