static struct { char name[32]; } stypes[64]; static int st_n;
static int st_find(const char *n) {
    return strcmp(stypes[0].name, n);
}
int main(void) { return 0; }
