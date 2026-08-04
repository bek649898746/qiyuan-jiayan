void set(char *r) { r[1] = 'x'; }
int main() {
    char buf[8];
    int i;
    for (i = 0; i < 8; i = i + 1) { buf[i] = '.'; }
    set(buf);
    if (buf[0] != '.') return 1;
    if (buf[1] != 'x') return 2;
    if (buf[2] != '.') return 3;
    return 0;
}
