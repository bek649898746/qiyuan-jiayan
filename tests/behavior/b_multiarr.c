// @EXPECTED exit:0
// @EXPECTED out:0 1 2 3
int printf(const char*, ...);
int main() {
    int m[3][4];
    int i, j, n = 0;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 4; j++)
            m[i][j] = i * 10 + j;
    for (i = 0; i < 3; i++) {
        printf("%d %d %d %d\n", m[i][0], m[i][1], m[i][2], m[i][3]);
    }
    printf("%d\n", m[2][3]);
    int flat[2][3][2];
    n = 0;
    for (i = 0; i < 2; i++)
        for (j = 0; j < 3; j++)
            flat[i][j][0] = n++;
    printf("%d %d %d\n", flat[0][0][0], flat[1][2][0], flat[1][0][0]);
    return 0;
}
