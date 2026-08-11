// @EXPECTED exit: 0
// 汇编本身: __asm("指令") 字符串汇编 (bin_mode)
// 生成 hlt 序列: 0xF4 0x90 0xC3
int main(void) {
    __asm("hlt");
    __asm("nop");
    __asm("ret");
    return 0;
}
