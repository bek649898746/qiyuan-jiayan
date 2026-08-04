static struct { char name[32]; int label; int defined; } func_tbl[64];
static int func_n;
static int ffind(const char *name) {
    int i;
    for (i = 0; i < func_n; i++) if (!strcmp(func_tbl[i].name, name)) return i;
    return -1;
}
int main(void) {
    int r;
    r = ffind("x");
    if (r != -1) return 1;
    return 0;
}
