int get(int a, int b, int *r) { return r[2]; }
int main() {
    int x[5];
    x[2] = 42;
    return get(1, 2, x) - 42;
}
