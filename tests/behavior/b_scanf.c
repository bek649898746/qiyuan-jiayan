// @EXPECTED exit:0
// @EXPECTED in:10 20
// @EXPECTED out:10 20
// scanf 变参输入 (fix 2026-08-12)
int printf(const char*, ...);
int scanf(const char*, ...);
int main() {
    int a = 0, b = 0;
    int r = scanf("%d %d", &a, &b);
    printf("%d %d %d\n", a, b, r);
    return 0;
}
