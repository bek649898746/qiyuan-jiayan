// @EXPECTED exit:0
static struct { char name[32]; } stypes[64]; static int st_n;
static int st_find(const char *n) {
    int i;
    int c;
    for (i = 0; i < st_n; i = i + 1) {
        c = stypes[i].name[0];
        if (c == 0) return i;
    }
    return -1;
}
int main(void) { return 0; }
