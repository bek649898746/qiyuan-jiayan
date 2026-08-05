int printf(const char*, ...);
double pow(double, double);
double fabs(double);
double fmod(double, double);
int main() {
    printf("%.1f\n", pow(2.0, 10));
    printf("%.1f\n", pow(2.0, 3));
    printf("%.1f\n", pow(2.0, 0));
    printf("%.1f\n", fabs(-5.5));
    printf("%.1f\n", fmod(17.5, 4.0));
    printf("%.6f\n", pow(2.0, 10));
    return 0;
}
