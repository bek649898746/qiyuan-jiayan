// @EXPECTED exit:0
int sum5(int a, int b, int c, int d, int e) { return a * 10000 + b * 1000 + c * 100 + d * 10 + e; }
int twice(int x) { return x * 2; }
int main() {
    return sum5(1, 2, 3, twice(4), 5) - 12385;
}
