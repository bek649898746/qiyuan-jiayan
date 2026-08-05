int printf(const char*, ...);
int main() {
    printf("%X\n", 48879);
    printf("%x\n", 48879);
    printf("%08x\n", 48879);
    printf("%#x\n", 48879);
    return 0;
}
