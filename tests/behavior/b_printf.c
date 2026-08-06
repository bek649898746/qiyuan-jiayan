// @EXPECTED exit:0
// @EXPECTED out:42 hello Z
int printf(const char*, ...);
int main() {
    printf("%d %s %c\n", 42, "hello", 'Z');
    printf("%5d|\n", 42);
    printf("%-5d|\n", 42);
    printf("%d\n", -42);
    printf("100%%\n");
    printf("%s\n", "嵌套" "拼接");
    printf("%d %d %d\n", 1, 2, 3);
    return 0;
}
