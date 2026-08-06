// @EXPECTED exit:0
int f(int n) {
    static int calls;
    calls = calls + 1;
    if (n <= 0) return 0;
    return f(n - 1) + calls;
}
int main() {
    return f(2) - 6;
}
