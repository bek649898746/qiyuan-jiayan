// @EXPECTED exit:0
int main() {
    int fp;
    char wbuf[4000];
    char rbuf[4000];
    int i;
    fp = fopen("qcc_test_big.txt", "w");
    if (fp == 0) return 1;
    i = 0;
    while (i < 4000) {
        wbuf[i] = i % 128;
        i = i + 1;
    }
    fwrite(wbuf, 1, 4000, fp);
    fclose(fp);
    fp = fopen("qcc_test_big.txt", "r");
    if (fp == 0) return 2;
    fread(rbuf, 1, 4000, fp);
    fclose(fp);
    i = 0;
    while (i < 4000) {
        if (rbuf[i] != (i % 128)) return 3;
        i = i + 1;
    }
    return 0;
}
