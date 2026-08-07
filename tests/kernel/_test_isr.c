// 验证 __isr_ 编译: 裸函数 + iretq
// qcc_x86_new.exe -bin tests/kernel/_test_isr.c -o scratch_test/_test_isr.bin

空 serial_init(整 com1) {
    outb(com1 + 1, 0); outb(com1 + 3, 0x80);
    outb(com1 + 0, 1); outb(com1 + 1, 0);
    outb(com1 + 3, 3); outb(com1 + 2, 0xC7);
    outb(com1 + 4, 0x0B);
}
空 serial_wait(整 com1) { 循环 ((inb(com1 + 5) & 0x20) == 0) { } }
空 serial_putc(整 com1, 整 c) {
    若 (c == 10) { serial_wait(com1); outb(com1 + 0, 13); }
    serial_wait(com1); outb(com1 + 0, c);
}
空 serial_puts(整 com1, 字 *s) { 循环 (*s != 0) { serial_putc(com1, *s); s++; } }

// __isr_ 裸函数: 编译器不生成 push rbp/rbx/sub rsp, iretq 代替 ret
// 注意: 无栈帧 → 不能声明局部变量, 只能用寄存器和内建函数
空 __isr_kbd(空) {
    inb(0x60);        // 读扫描码（丢弃返回值）
    outb(0x20, 0x20); // EOI
}

空 _start(空) {
    整 c = 0x3F8;
    serial_init(c);
    serial_puts(c, "ISR COMPILE OK\n");
    循环 (1) { __asm(0xF4); }
}
