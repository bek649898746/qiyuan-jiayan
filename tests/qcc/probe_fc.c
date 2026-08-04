static struct { char name[32]; int label; int defined; } func_tbl[64];
static int func_n;
static int strcmp2(const char *a, const char *b) {
    int i;
    for (i = 0; i < 64; i = i + 1) if (!strcmp(func_tbl[i].name, b)) return i;
    return -1;
}
int main(void) {
    int r;
    r = strcmp2(func_tbl[0].name, "x");
    if (r != -1) return 1;
    return 0;
}
