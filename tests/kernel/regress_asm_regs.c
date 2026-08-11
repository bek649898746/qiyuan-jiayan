// @EXPECTED exit: 0
// 汇编本身: 寄存器/立即数指令编码
int main(void) {
    __asm("mov rax, 0x10");   /* 48 C7 C0 10 00 00 00 */
    __asm("add rax, 1");      /* 48 81 C0 01 00 00 00 */
    __asm("sub rax, 1");      /* 48 81 E8 01 00 00 00 */
    __asm("mov rbx, rax");    /* 48 89 C3 */
    __asm("push rax");        /* 50 */
    __asm("pop rax");         /* 58 */
    __asm("cli");             /* FA */
    __asm("sti");             /* FB */
    return 0;
}
