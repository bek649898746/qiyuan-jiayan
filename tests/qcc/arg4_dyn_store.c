// @EXPECTED exit:0
int get(int a, int b, int c, int *r) { return r[2]; }
int main() {
    int x[5];
    int i;
    i = 0;
    x[i] = 42;
    i = 2;
    return get(1, 2, 3, x) - x[2];
}
