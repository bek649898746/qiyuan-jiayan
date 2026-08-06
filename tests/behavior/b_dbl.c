int printf(const char*, ...);
double add(double a, double b) { return a + b; }
double mul(double a, double b) { return a * b; }
int main() {
    double a = 1.5;
    double b = 2.25;
    printf("%.2f\n", add(a, b));
    printf("%.2f\n", mul(a, b));
    printf("%.2f\n", add(1.0, 2.0));
    printf("%.2f\n", a - b);
    printf("%.2f\n", b / a);
    int n = 3;
    double d = 2.5;
    printf("%.2f\n", add(d, (double)n));
    return 0;
}
