int get(int a, int b, int c, int *r) { return a; }
int main() {
    int x[5];
    x[2] = 42;
    return get(1, 2, 3, x) - 1;
}
