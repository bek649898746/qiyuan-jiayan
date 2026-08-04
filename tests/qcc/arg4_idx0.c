int get(int a, int b, int c, int *r) { return r[0]; }
int main() {
    int x[5];
    x[0] = 55;
    return get(1, 2, 3, x) - 55;
}
