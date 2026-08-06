// @EXPECTED exit:0
int get(int *r) { return 7; }
int main() {
    int x[5];
    x[2] = 42;
    return get(x) - 7;
}
