// @EXPECTED exit:0
char first(char *s) { return s[0]; }
int main() {
    if (first("abc") != 'a') return 1;
    return 0;
}
