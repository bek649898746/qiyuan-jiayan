// 甲言裸机内核 v3 — COM1 串口 printk + VGA 双输出 + 键盘回显
// 构建: qcc_x86.exe -bin tests/kernel/kernel_serial.c -o scratch_test/kernel_serial.bin
// 验证: qemu -kernel ... -serial stdio

整 pos = 0;
整 com1 = 0x3F8;

// === COM1 串口驱动 (8250 UART) ===

空 serial_init(空) {
    outb(com1 + 1, 0);       // IER=0  禁用中断
    outb(com1 + 3, 0x80);    // LCR bit7=1 启用 DLAB
    outb(com1 + 0, 1);       // 波特率除数低字节 1→115200
    outb(com1 + 1, 0);       // 除数高字节 0
    outb(com1 + 3, 3);       // 8N1, DLAB=0
    outb(com1 + 2, 0xC7);    // FCR: 启用FIFO + 清空 + 14字节阈值
    outb(com1 + 4, 0x0B);    // MCR: DTR + RTS + OUT2
}

空 serial_wait(空) {
    循环 ((inb(com1 + 5) & 0x20) == 0) { }  // LSR bit5: THRE 发送就绪
}

空 serial_putc(整 c) {
    若 (c == 10) { serial_wait(); outb(com1 + 0, 13); }  // \n 前加 \r
    serial_wait();
    outb(com1 + 0, c);
}

空 serial_puts(字 *s) {
    循环 (*s != 0) { serial_putc(*s); s++; }
}

// === VGA 文本输出 ===

空 put_c(整 c) {
    *(无 短*)(0xB8000 + pos * 2) = (无 短)(0x0200 | (无 短)c);
    pos++;
}

空 put_s(字 *s) {
    循环 (*s != 0) { put_c(*s); s++; }
}

空 put_nl(空) {
    pos = ((pos / 80) + 1) * 80;
}

// === 双通道 printk ===

空 printk(字 *s) {
    put_s(s);
    serial_puts(s);
}

空 printk_nl(空) {
    put_nl();
    serial_puts("\r\n");
}

// === 键盘 (Set 1 扫描码 → ASCII) ===

整 sc_to_ascii(整 sc) {
    若 (sc == 0x10) { 返 113; }  // q
    若 (sc == 0x11) { 返 119; }  // w
    若 (sc == 0x12) { 返 101; }  // e
    若 (sc == 0x13) { 返 114; }  // r
    若 (sc == 0x14) { 返 116; }  // t
    若 (sc == 0x15) { 返 121; }  // y
    若 (sc == 0x16) { 返 117; }  // u
    若 (sc == 0x17) { 返 105; }  // i
    若 (sc == 0x18) { 返 111; }  // o
    若 (sc == 0x19) { 返 112; }  // p
    若 (sc == 0x1E) { 返 97; }   // a
    若 (sc == 0x1F) { 返 115; }  // s
    若 (sc == 0x20) { 返 100; }  // d
    若 (sc == 0x21) { 返 102; }  // f
    若 (sc == 0x22) { 返 103; }  // g
    若 (sc == 0x23) { 返 104; }  // h
    若 (sc == 0x24) { 返 106; }  // j
    若 (sc == 0x25) { 返 107; }  // k
    若 (sc == 0x26) { 返 108; }  // l
    若 (sc == 0x2C) { 返 122; }  // z
    若 (sc == 0x2D) { 返 120; }  // x
    若 (sc == 0x2E) { 返 99; }   // c
    若 (sc == 0x2F) { 返 118; }  // v
    若 (sc == 0x30) { 返 98; }   // b
    若 (sc == 0x31) { 返 110; }  // n
    若 (sc == 0x32) { 返 109; }  // m
    若 (sc == 0x02) { 返 49; }   // 1
    若 (sc == 0x03) { 返 50; }   // 2
    若 (sc == 0x04) { 返 51; }   // 3
    若 (sc == 0x05) { 返 52; }   // 4
    若 (sc == 0x06) { 返 53; }   // 5
    若 (sc == 0x07) { 返 54; }   // 6
    若 (sc == 0x08) { 返 55; }   // 7
    若 (sc == 0x09) { 返 56; }   // 8
    若 (sc == 0x0A) { 返 57; }   // 9
    若 (sc == 0x0B) { 返 48; }   // 0
    若 (sc == 0x39) { 返 32; }   // 空格
    若 (sc == 0x1C) { 返 10; }   // 回车
    返 0;
}

// === 入口 ===

空 _start(空) {
    serial_init();
    printk("JIAYAN KERNEL v3");
    printk_nl();
    printk("COM1 OK | SEED:828");
    printk_nl();
    printk(">");

    循环 (1) {
        整 s = inb(0x64);
        若 ((s & 1) != 0) {
            整 sc = inb(0x60);
            若 ((sc & 0x80) == 0) {
                整 c = sc_to_ascii(sc);
                若 (c == 10) { printk_nl(); printk(">"); }
                否则 若 (c != 0) { put_c(c); serial_putc(c); }
            }
        }
    }
}
