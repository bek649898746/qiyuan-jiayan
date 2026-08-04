int g(int x) { return x; }
int f(int n) {
    return g(n - 1) + 1;
}
int main() {
    return f(3) - 3;
}
