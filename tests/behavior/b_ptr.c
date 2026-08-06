int printf(const char*, ...);
int main() {
    int x = 42;
    int *p = &x;
    *p = 99;
    printf("%d\n", x);
    printf("%d\n", *p);
    int a[4];
    a[0] = 7; a[1] = 8; a[2] = 9; a[3] = 10;
    int *q = a;
    printf("%d\n", q[2]);
    int sum = 0, i;
    for (i = 0; i < 4; i++) sum += a[i];
    printf("%d\n", sum);
    return 0;
}
