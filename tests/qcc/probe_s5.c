// @EXPECTED exit:0
static struct { char name[32]; } stypes[64]; static int st_n;
static int st_find(const char *n) {
    int i;
    for (i = 0; i < st_n; i = i + 1) if (st_n == i) return i;
    return -1;
}
int main(void) { return 0; }
