void set(int a, int b, int c, int *r) { r[2] = 42; }
int main() {
    int x[5];
    set(1, 2, 3, x);
    return x[2] - 42;
}
