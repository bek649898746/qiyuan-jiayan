int strlen2(char *s) { int n; n = 0; while (s[n] != 0) { n = n + 1; } return n; }
int main() {
    char msg[16];
    int i;
    for (i = 0; i < 15; i = i + 1) { msg[i] = 'a'; }
    msg[15] = 0;
    return strlen2(msg) - 15;
}
