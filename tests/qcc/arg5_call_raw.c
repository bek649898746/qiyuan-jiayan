// @EXPECTED exit:nonzero
int sum5(int a, int b, int c, int d, int e) { return a + b + c + d + e; }
int twice(int x) { return x * 2; }
int main() {
    return sum5(1, 2, 3, twice(4), 5);
}
