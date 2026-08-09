// @EXPECTED out:OK
// 回归: printf %08x/%#x/%#X 宽度+前缀 (fix 2026-08-09 审计#7)
int main(void) {
    printf("%08x\n", 0x2a);
    printf("%#x\n", 0x2a);
    printf("%#X\n", 0x2a);
    printf("%#08x\n", 0x2a);
    printf("%8x\n", 0x2a);
    printf("%#8x\n", 0x2a);
    printf("%x\n", 0);
    printf("%08x\n", 0);
    printf("%#x\n", 0);
    printf("OK\n");
    return 0;
}
