// 甲言裸机内核 v5 — 零变量最小版（绕过 bin 模式全局寻址 bug）
// 构建: qcc_x86_new.exe -bin tests/kernel/kernel_noglob.c -o scratch_test/kernel_noglob.bin

空 serial_init(整 com1) {
    outb(com1 + 1, 0);
    outb(com1 + 3, 0x80);
    outb(com1 + 0, 1);
    outb(com1 + 1, 0);
    outb(com1 + 3, 3);
    outb(com1 + 2, 0xC7);
    outb(com1 + 4, 0x0B);
}

空 serial_wait(整 com1) {
    循环 ((inb(com1 + 5) & 0x20) == 0) { }
}

空 serial_putc(整 com1, 整 c) {
    若 (c == 10) { serial_wait(com1); outb(com1 + 0, 13); }
    serial_wait(com1);
    outb(com1 + 0, c);
}

空 serial_puts(整 com1, 字 *s) {
    循环 (*s != 0) { serial_putc(com1, *s); s++; }
}

空 _start(空) {
    整 c = 0x3F8;
    serial_init(c);
    serial_puts(c, "JIAYAN v5 NOGLOB\r\nSEED:828\r\n");

    // VGA 直接写入 3 个字符
    *(无 短*)(0xB8000 + 0) = (无 短)(0x024A);   // 'J' green
    *(无 短*)(0xB8000 + 2) = (无 短)(0x0259);   // 'Y' green
    *(无 短*)(0xB8000 + 4) = (无 短)(0x0221);   // '!' green

    // 死循环
    循环 (1) { __asm(0xF4); }
}
