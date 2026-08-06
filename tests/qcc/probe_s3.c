// @EXPECTED exit:0
static struct { char name[32]; } stypes[64]; static int st_n;
static int st_find(const char *n) {
    int r;
    r = stypes[0].name[0];
    return r;
}
int main(void) { return 0; }
