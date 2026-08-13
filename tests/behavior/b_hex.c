// @EXPECTED exit:0
// @EXPECTED out:BEEF
// %08x 零填充 与 %#x 0x 前缀已实现 (emit_hex_prefix_pad, 2026-08-09 审计#7) — expected 已匹配实现
int printf(const char*, ...);
int main() {
    printf("%X\n", 48879);
    printf("%x\n", 48879);
    printf("%08x\n", 48879);
    printf("%#x\n", 48879);
    return 0;
}
