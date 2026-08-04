int main() {
    int fp;
    char buf[16];
    fp = fopen("qcc_test_file.txt", "w");
    if (fp == 0) return 1;
    fwrite("hello-file", 1, 10, fp);
    fclose(fp);
    fp = fopen("qcc_test_file.txt", "r");
    if (fp == 0) return 2;
    fread(buf, 1, 10, fp);
    fclose(fp);
    if (buf[0] != 'h') return 3;
    if (buf[5] != '-') return 4;
    return 0;
}
