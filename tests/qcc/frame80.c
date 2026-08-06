// @EXPECTED exit:0
int get(int a, int b, int c, int d) { return 7; }
int main() {
    int i;
    int j;
    int k;
    int l;
    int m;
    i = 1; j = 2; k = 3; l = 4; m = 5;
    return get(1, 2, 3, 4) - 7;
}
