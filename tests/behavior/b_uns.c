int printf(const char*, ...);
int main() {
    printf("%d\n", 0xFFFFFFFFu >= 1u);
    printf("%d\n", 1u >= 0xFFFFFFFFu);
    printf("%d\n", 1u <= 0xFFFFFFFFu);
    printf("%d\n", 0xFFFFFFFFu > 1u);
    printf("%d\n", 1u < 0xFFFFFFFFu);
    printf("%u\n", 0xFFFFFFFFu / 2u);
    printf("%u\n", 0xFFFFFFFFu % 10u);
    printf("%u\n", 0xFFFFFFFFu >> 1);
    unsigned int x = 0xFFFFFFFFu;
    printf("%d\n", x > 0);
    printf("%d\n", x == 0xFFFFFFFFu);
    return 0;
}
