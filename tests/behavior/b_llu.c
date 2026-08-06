// @EXPECTED exit:0
// @EXPECTED out:18446744073709551615
int printf(const char*, ...);
int main() {
    printf("%llu\n", 18446744073709551615ULL);
    printf("%llu\n", 3000000000ULL);
    unsigned long long x = 12345678901234567890ULL;
    printf("%llu\n", x);
    unsigned long long a = 18446744073709551614ULL;
    a++;
    printf("%llu\n", a);
    return 0;
}
