// @EXPECTED exit:0
static struct { char name[32]; int val; } tbl[8];
static int g_n;
int main(void) {
    g_n = 2;
    tbl[0].val = 10;
    if (tbl[0].val != 10) return 1;
    tbl[g_n].val = 30;
    if (tbl[2].val != 30) return 2;
    return 0;
}
