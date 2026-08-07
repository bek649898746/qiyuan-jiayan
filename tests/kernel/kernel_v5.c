// 甲言裸机内核 v5 — 全功能版 (栈上 context, 零全局变量)
// VGA 双输出 + COM1 串口 + 键盘轮询 + PIC 框架 + hlt 省电
// 构建: qcc_x86_new.exe -bin tests/kernel/kernel_v5.c -o scratch_test/kernel_v5.bin
// 验证: python scripts/stitch_kernel.py scratch_test/kernel_v5.bin <entry>

// === COM1 串口 ===

空 serial_init(整 com1) {
    outb(com1 + 1, 0); outb(com1 + 3, 0x80);
    outb(com1 + 0, 1); outb(com1 + 1, 0);
    outb(com1 + 3, 3); outb(com1 + 2, 0xC7);
    outb(com1 + 4, 0x0B);
}

空 serial_wait(整 com1) {
    循环 ((inb(com1 + 5) & 0x20) == 0) { }
}

空 serial_putc(整 com1, 整 c) {
    若 (c == 10) { serial_wait(com1); outb(com1 + 0, 13); }
    serial_wait(com1); outb(com1 + 0, c);
}

空 serial_puts(整 com1, 字 *s) {
    循环 (*s != 0) { serial_putc(com1, *s); s++; }
}

// === VGA 文本 ===

空 vga_putc(整 *pos, 整 c) {
    *(无 短*)(0xB8000 + (*pos) * 2) = (无 短)(0x0200 | (无 短)c);
    *pos = *pos + 1;
}

空 vga_puts(整 *pos, 字 *s) {
    循环 (*s != 0) { vga_putc(pos, *s); s++; }
}

空 vga_nl(整 *pos) {
    *pos = ((*pos / 80) + 1) * 80;
}

// === 双通道 printk ===

空 printk(整 *pos, 整 com1, 字 *s) {
    vga_puts(pos, s);
    serial_puts(com1, s);
}

空 printk_nl(整 *pos, 整 com1) {
    vga_nl(pos);
    serial_puts(com1, "\n");
}

// === PIC 8259A ===

空 pic_init(空) {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xFF); outb(0xA1, 0xFF);
}

// === 键盘 (Set 1 扫描码 → ASCII) ===

整 sc_to_ascii(整 sc) {
    若 (sc == 0x10) { 返 113; } 若 (sc == 0x11) { 返 119; }
    若 (sc == 0x12) { 返 101; } 若 (sc == 0x13) { 返 114; }
    若 (sc == 0x14) { 返 116; } 若 (sc == 0x15) { 返 121; }
    若 (sc == 0x16) { 返 117; } 若 (sc == 0x17) { 返 105; }
    若 (sc == 0x18) { 返 111; } 若 (sc == 0x19) { 返 112; }
    若 (sc == 0x1E) { 返 97; }  若 (sc == 0x1F) { 返 115; }
    若 (sc == 0x20) { 返 100; } 若 (sc == 0x21) { 返 102; }
    若 (sc == 0x22) { 返 103; } 若 (sc == 0x23) { 返 104; }
    若 (sc == 0x24) { 返 106; } 若 (sc == 0x25) { 返 107; }
    若 (sc == 0x26) { 返 108; } 若 (sc == 0x2C) { 返 122; }
    若 (sc == 0x2D) { 返 120; } 若 (sc == 0x2E) { 返 99; }
    若 (sc == 0x2F) { 返 118; } 若 (sc == 0x30) { 返 98; }
    若 (sc == 0x31) { 返 110; } 若 (sc == 0x32) { 返 109; }
    若 (sc == 0x02) { 返 49; }  若 (sc == 0x03) { 返 50; }
    若 (sc == 0x04) { 返 51; }  若 (sc == 0x05) { 返 52; }
    若 (sc == 0x06) { 返 53; }  若 (sc == 0x07) { 返 54; }
    若 (sc == 0x08) { 返 55; }  若 (sc == 0x09) { 返 56; }
    若 (sc == 0x0A) { 返 57; }  若 (sc == 0x0B) { 返 48; }
    若 (sc == 0x39) { 返 32; }  若 (sc == 0x1C) { 返 10; }
    返 0;
}

// === 入口 ===

空 _start(空) {
    整 pos = 0;
    整 com1 = 0x3F8;

    serial_init(com1);
    pic_init();

    printk(&pos, com1, "JIAYAN KERNEL v5");
    printk_nl(&pos, com1);
    printk(&pos, com1, "PIC OK | COM1 OK | SEED:828");
    printk_nl(&pos, com1);
    printk(&pos, com1, ">");

    循环 (1) {
        整 s = inb(0x64);
        若 ((s & 1) != 0) {
            整 sc = inb(0x60);
            若 ((sc & 0x80) == 0) {
                整 c = sc_to_ascii(sc);
                若 (c == 10) { printk_nl(&pos, com1); printk(&pos, com1, ">"); }
                否则 若 (c != 0) { vga_putc(&pos, c); serial_putc(com1, c); }
            }
        }
        否则 {
            __asm(0xF4);  // hlt
        }
    }
}
