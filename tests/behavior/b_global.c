// @EXPECTED exit:0
// @EXPECTED out:10
int printf(const char*, ...);
int g = 10;
long long gl = 3000000000LL;
double gd = 3.14;
int main() {
    printf("%d\n", g);
    printf("%lld\n", gl);
    printf("%.2f\n", gd);
    g += 5;
    gl *= 2;
    printf("%d %lld\n", g, gl);
    return 0;
}
