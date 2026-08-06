// @EXPECTED exit:0
void set(int *r, int i) { r[i] = 77; }
int main() {
    int x[5];
    set(x, 4);
    return x[4] - 77;
}
