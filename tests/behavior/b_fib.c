// @EXPECTED exit:0
// @EXPECTED out:55
int printf(const char*, ...);
int fib(int n) { if (n < 2) return n; return fib(n - 1) + fib(n - 2); }
int fact(int n) { int r = 1, i; for (i = 2; i <= n; i++) r = r * i; return r; }
int main() {
    printf("%d\n", fib(10));
    printf("%d\n", fib(20));
    printf("%d\n", fact(5));
    printf("%d\n", fact(10));
    return 0;
}
