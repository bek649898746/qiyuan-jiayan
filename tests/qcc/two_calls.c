// @EXPECTED exit:0
int id(int x) { return x; }
int main() {
    return id(1) + id(0) - 1;
}
