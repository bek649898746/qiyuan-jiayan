// @EXPECTED exit:0
int main() {
    int x[5000];
    int i;
    for (i = 0; i < 5000; i = i + 1) { x[i] = i; }
    return x[4999] - 4999;
}
