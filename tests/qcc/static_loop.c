// @EXPECTED exit:0
int counter(void) {
    static int n;
    n = n + 1;
    return n;
}
int main() {
    int i;
    int s;
    s = 0;
    for (i = 0; i < 5; i = i + 1) { s = s + counter(); }
    return s - 15;
}
