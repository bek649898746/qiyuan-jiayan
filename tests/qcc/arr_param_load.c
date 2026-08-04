int get(int *r) { return r[2]; }
int main() {
    int a[5];
    a[2] = 42;
    return get(a) - 42;
}
