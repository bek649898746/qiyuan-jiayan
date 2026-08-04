int counter(void) {
    static int n;
    n = n + 1;
    return n;
}
int main() {
    int a;
    int b;
    a = counter();
    b = counter();
    return (a + b) - 3;
}
