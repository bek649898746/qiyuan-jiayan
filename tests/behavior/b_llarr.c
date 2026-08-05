int printf(const char*, ...);
int main() {
    long long a[3];
    a[0] = 3000000000LL;
    a[1] = -3000000000LL;
    a[2] = 1LL << 40;
    printf("%lld %lld %lld\n", a[0], a[1], a[2]);
    long long sum = a[0] + a[1] + a[2];
    printf("%lld\n", sum);
    long long g = 0;
    int k;
    for (k = 0; k < 3; k++) g += a[k];
    printf("%lld\n", g);
    return 0;
}
