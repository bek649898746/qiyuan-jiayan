int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}
int main() {
    return fib(2) * 100 + fib(1) * 10 + fib(0) - 110;
}
