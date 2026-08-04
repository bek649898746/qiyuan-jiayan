int add2(int a, int b) { return a + b; }
int main() {
    int x[6];
    int i;
    for (i = 0; i < 6; i = i + 1) { x[i] = i; }
    return add2(add2(1, 2), add2(3, 4)) - 10;
}
