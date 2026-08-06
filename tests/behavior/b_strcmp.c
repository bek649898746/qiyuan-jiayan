int printf(const char*, ...);
int strcmp2(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}
int main() {
    char s[32];
    s[0] = 'h'; s[1] = 'i'; s[2] = 0;
    printf("%s\n", s);
    printf("%d\n", strcmp2("abc", "abc"));
    printf("%d\n", strcmp2("abc", "abd"));
    printf("%d\n", strcmp2("abd", "abc"));
    int a = 0, b = 0;
    a = 5; b = a; a = b + 1;
    printf("%d %d\n", a, b);
    return 0;
}
