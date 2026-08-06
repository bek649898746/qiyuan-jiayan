// @EXPECTED exit:0
int main() {
    int r[5];
    r[0] = 2;
    r[r[0]] = 42;
    return r[2] - 42;
}
