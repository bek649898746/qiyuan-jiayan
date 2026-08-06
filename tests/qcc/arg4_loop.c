// @EXPECTED exit:0
int get(int a, int b, int c, int *r) { return r[2]; }
int main() {
    int x[5];
    int i;
    for (i = 0; i < 5; i = i + 1) { x[i] = i; }
    return get(1, 2, 3, x) - 2;
}
