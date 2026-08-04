int get(int a, int b, int c, int *r) { return 7; }
int main() {
    int x[5];
    x[2] = 42;
    return get(1, 2, 3, x) - 7;
}
