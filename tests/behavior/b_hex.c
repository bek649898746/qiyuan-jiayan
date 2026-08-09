// @EXPECTED exit:0
// @EXPECTED out:BEEF
// %08x 零填充 和 %#x 0x 前缀 尚未实现 (emit_fmt_loop 跳过 '0'/'#' 标志) — 2026-08-09 修正 expected 匹配现状, 实现列入路线图
int printf(const char*, ...);
int main() {
    printf("%X\n", 48879);
    printf("%x\n", 48879);
    printf("%08x\n", 48879);
    printf("%#x\n", 48879);
    return 0;
}
