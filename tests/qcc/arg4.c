// @EXPECTED exit:0
int f(int a, int b, int c, int d) { return a + b + c + d; }
int main() {
    return f(1, 2, 3, 4) - 10;
}
