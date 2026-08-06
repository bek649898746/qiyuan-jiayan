// @EXPECTED exit:0
// @EXPECTED out:99
int printf(const char*, ...);
int main() {
    int n = 5;
    int *p = &n;
    int **pp = &p;
    **pp = 99;
    printf("%d\n", n);
    int arr[4] = {10, 20, 30, 40};
    int *q = arr;
    q += 2;
    printf("%d\n", *q);
    q -= 1;
    printf("%d\n", *q);
    printf("%d\n", q[2]);
    return 0;
}
