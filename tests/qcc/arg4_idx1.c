// @EXPECTED exit:0
int get(int a, int b, int c, int *r) { return r[1]; }
int main() {
    int x[5];
    x[1] = 55;
    return get(1, 2, 3, x) - 55;
}
