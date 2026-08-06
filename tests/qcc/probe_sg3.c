// @EXPECTED exit:0
static struct { char name[32]; int val; } tbl[8];
int main(void) {
    tbl[0].val = 10;
    if (tbl[0].val != 10) return 1;
    tbl[1].val = 20;
    if (tbl[1].val != 20) return 2;
    return 0;
}
