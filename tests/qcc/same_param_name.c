int len1(char *s) { int n; n = 0; while (s[n] != 0) { n = n + 1; } return n; }
int len2(char *s) { int n; n = 0; while (s[n] != 0) { n = n + 1; } return n; }
int main() {
    char a[4];
    char b[4];
    a[0] = 'x'; a[1] = 'y'; a[2] = 0;
    b[0] = 'p'; b[1] = 'q'; b[2] = 'r'; b[3] = 0;
    return (len1(a) - 2) + (len2(b) - 3);
}
