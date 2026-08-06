int printf(const char*, ...);
int main() {
    int i = -7;
    long long x = (long long)i;
    printf("%lld\n", x);
    int j = 7;
    long long y = (long long)j;
    printf("%lld\n", y);
    long long z = (long long)i + 1000000000000LL;
    printf("%lld\n", z);
    unsigned int u = 0xFFFFFFFFu;
    long long w = (long long)u;
    printf("%lld\n", w);
    return 0;
}
