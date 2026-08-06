int printf(const char*, ...);
int main() {
    int x = 42;
    switch (x % 4) {
        case 0: printf("zero\n"); break;
        case 1: printf("one\n"); break;
        case 2: printf("two\n"); break;
        default: printf("other\n"); break;
    }
    int i = 1;
    switch (i) { case 1: printf("a\n"); case 2: printf("b\n"); default: printf("c\n"); }
    return 0;
}
