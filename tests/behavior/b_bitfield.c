// @EXPECTED exit:0
// @EXPECTED out:5 17 200
int printf(const char*, ...);
struct Flags { unsigned int a : 3; unsigned int b : 5; unsigned int c : 8; };
int main() {
    struct Flags f;
    f.a = 5;
    f.b = 17;
    f.c = 200;
    printf("%u %u %u\n", f.a, f.b, f.c);
    f.a = 0;
    f.b = 31;
    f.c = 255;
    printf("%u %u %u\n", f.a, f.b, f.c);
    return 0;
}
