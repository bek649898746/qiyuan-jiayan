// @EXPECTED exit:0
static struct { int val; int b2; } tbl[8];
int main(void) {
    tbl[0].val = 10;
    if (tbl[0].val != 10) return 1;
    return 0;
}
