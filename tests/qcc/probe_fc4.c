static struct { char name[32]; int label; int defined; } func_tbl[64];
static int func_n;
static int ffind(const char *name) {
    for (int i = 0; i < func_n; i++) if (!strcmp(func_tbl[i].name, name)) return i;
    if (func_n >= 64) return -1;
    strcpy(func_tbl[func_n].name, name);
    func_tbl[func_n].label = 42;
    return func_n++;
}
int main(void) {
    if (ffind("abc") != 0) return 1;
    if (func_n != 1) return 2;
    if (func_tbl[0].label != 42) return 3;
    return 0;
}
