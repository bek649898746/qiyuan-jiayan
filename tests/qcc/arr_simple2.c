// @EXPECTED exit:0
int main() {
    int x[5];
    x[2] = 42;
    return x[2] - 42;
}
