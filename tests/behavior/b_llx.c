int printf(const char*, ...);
int main() {
    printf("%llx\n", 0x123456789abcdef0LL);
    printf("%llx\n", -1LL);
    printf("%llx\n", 305419896LL);
    printf("%x\n", 48879);
    return 0;
}
