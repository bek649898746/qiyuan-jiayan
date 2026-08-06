// @EXPECTED exit:0
int compute(int n) {
    int s;
    int i;
    s = 0;
    for (i = 0; i < n; i = i + 1) {
        s = s + i * 2;
        s = s - 1;
        s = s + 3;
        s = s - 2;
        s = s + 5;
        s = s - 4;
        s = s + 7;
        s = s - 6;
        s = s + 11;
        s = s - 10;
        s = s + 13;
        s = s - 12;
        s = s + 17;
        s = s - 16;
        s = s + 19;
        s = s - 18;
    }
    return s;
}
int main() {
    int x[8];
    int i;
    for (i = 0; i < 8; i = i + 1) { x[i] = i + 1; }
    return compute(10) - x[7] - 142;
}
