int printf(const char*, ...);
int main() {
    printf("%.2f\n", 3.14159);
    printf("%5.1f|\n", 3.14159);
    printf("%.0f\n", 2.5);
    printf("%.1f\n", -0.5);
    printf("%f\n", 1.0 / 3.0);
    return 0;
}
