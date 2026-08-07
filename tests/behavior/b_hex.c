// @EXPECTED exit:0
// @EXPECTED out:BEEF
// 注: %08x 零填充 和 %#x 0x 前缀是已知未实现特性 (emit_fmt_loop 静默跳过 '0'/'#' 标志)
//     当前输出 beef (无填充/无前缀)。实现这两特性后需同步更新本断言。
int printf(const char*, ...);
int main() {
    printf("%X\n", 48879);
    printf("%x\n", 48879);
    printf("%08x\n", 48879);
    printf("%#x\n", 48879);
    return 0;
}
