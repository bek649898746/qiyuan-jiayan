// @EXPECTED exit:0
static struct { char name[32]; int val; } tbl[8];
int main(void) {
    tbl[0].name[0] = 'X';
    if (tbl[0].name[0] != 'X') return 1;
    tbl[1].name[3] = 'Y';
    if (tbl[1].name[3] != 'Y') return 2;
    return 0;
}
