// @EXPECTED exit:nonzero
int get(int a, int b, int c, int *r) { return (int)r; }
int main() {
    int x[5];
    x[0] = 0;
    return get(1, 2, 3, x);
}
