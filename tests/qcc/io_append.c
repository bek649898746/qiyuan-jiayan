// @EXPECTED exit:0
int main() {
    int fp;
    char buf[8];
    fp = fopen("qcc_test_append.txt", "w");
    if (fp == 0) return 1;
    fwrite("AAA", 1, 3, fp);
    fclose(fp);
    fp = fopen("qcc_test_append.txt", "a");
    if (fp == 0) return 2;
    fwrite("BBB", 1, 3, fp);
    fclose(fp);
    fp = fopen("qcc_test_append.txt", "r");
    if (fp == 0) return 3;
    fread(buf, 1, 6, fp);
    fclose(fp);
    if (buf[0] != 'A') return 4;
    if (buf[3] != 'B') return 5;
    if (buf[5] != 'B') return 6;
    return 0;
}
