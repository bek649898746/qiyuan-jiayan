// @EXPECTED exit:0
int f(int n) {
    if (n == 0) return 0;
    return f(n - 1) + 1;
}
int main() {
    return f(3) - 3;
}
