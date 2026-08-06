// @EXPECTED exit:0
int set5(int a, int b, int c, int d, int e, int *r) {
    r[e] = a + b + c + d + e;
    return r[e];
}
int main() {
    int x[10];
    int i;
    for (i = 0; i < 10; i = i + 1) { x[i] = 0; }
    if (set5(1, 2, 3, 4, 5, x) != 15) return 1;
    if (x[5] != 15) return 2;
    return 0;
}
