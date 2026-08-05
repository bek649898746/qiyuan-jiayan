int printf(const char*, ...);
int main() {
    printf("%lld\n", 3000000000LL);
    printf("%lld\n", -3000000000LL);
    printf("%lld\n", -9223372036854775807LL);
    printf("%lld\n", 1LL << 40);
    return 0;
}
