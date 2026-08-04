void set(int *r) {
    r[2] = 42;
}
int main() {
    int a[5];
    set(a);
    return a[2] - 42;
}
