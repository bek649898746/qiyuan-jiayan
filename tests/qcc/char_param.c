void set(char *r) { r[1] = 'x'; }
int main() {
    char buf[8];
    int i;
    for (i = 0; i < 8; i = i + 1) { buf[i] = '.'; }
    set(buf);
    return 0;
}
