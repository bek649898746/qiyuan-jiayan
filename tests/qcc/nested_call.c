// @EXPECTED exit:0
int add2(int a, int b) { return a + b; }
int main() {
    return add2(add2(1, 2), add2(3, 4)) - 10;
}
