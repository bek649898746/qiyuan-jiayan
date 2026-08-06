// @EXPECTED exit:0
int sum5(int a, int b, int c, int d, int e) { return a + b + c + d + e; }
int main() {
    return sum5(1, 2, 3, 4, 5) - 15;
}
