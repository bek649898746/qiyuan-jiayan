// @EXPECTED exit:0
int get(int a, int b, int c, int d) { return 7; }
int main() {
    int x[5];
    x[2] = 42;
    return get(1, 2, 3, 4) - 7;
}
