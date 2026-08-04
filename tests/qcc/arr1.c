int get(int a, int b, int c, int d) { return 7; }
int main() {
    int x[1];
    x[0] = 42;
    return get(1, 2, 3, 4) - 7;
}
