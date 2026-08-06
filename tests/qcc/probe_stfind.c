// @EXPECTED exit:0
static struct { char name[32]; } stypes[64]; static int st_n;
static int st_find(const char *n) {
    for (int i = 0; i < st_n; i++) if (!strcmp(stypes[i].name, n)) return i;
    return -1;
}
int main(void) { return 0; }
