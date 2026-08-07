// 甲言裸机内核 v4 — PIC 中断框架就绪 + 轮询键盘 + COM1/VGA 双输出
// Gate 1: PIC 8259A 重映射完成，中断向量已分配，IRQ 已屏蔽（轮询安全）
// Gate 1 遗留: IDT 表初始化需要编译器支持函数指针地址或手写汇编桩
// 构建: qcc_x86_new.exe -bin tests/kernel/kernel_pic.c -o scratch_test/kernel_pic.bin

整 pos = 0;
整 com1 = 0x3F8;

// === COM1 串口 (8250 UART) ===

空 serial_init(空) {
    outb(com1 + 1, 0);       // IER=0
    outb(com1 + 3, 0x80);    // DLAB=1
    outb(com1 + 0, 1);       // 115200
    outb(com1 + 1, 0);
    outb(com1 + 3, 3);       // 8N1, DLAB=0
    outb(com1 + 2, 0xC7);    // FIFO: 启用+清空+14字节阈值
    outb(com1 + 4, 0x0B);    // DTR+RTS+OUT2
}

空 serial_wait(空) {
    循环 ((inb(com1 + 5) & 0x20) == 0) { }
}

空 serial_putc(整 c) {
    若 (c == 10) { serial_wait(); outb(com1 + 0, 13); }
    serial_wait();
    outb(com1 + 0, c);
}

空 serial_puts(字 *s) {
    循环 (*s != 0) { serial_putc(*s); s++; }
}

// === VGA 文本 ===

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

// === 双通道 ===

空 printk(字 *s) {
    put_s(s);
    serial_puts(s);
}

空 printk_nl(空) {
    put_nl();
    serial_puts("\r\n");
}

// === PIC 8259A 初始化 (中断框架就绪) ===
// IRQ0-7  → vector 0x20-0x27
// IRQ8-15 → vector 0x28-0x2F
// 全部屏蔽 (OCW1=0xFF)，轮询模式安全

空 pic_init(空) {
    // ICW1: 边沿触发 + 级联 + ICW4
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    // ICW2: 向量基址
    outb(0x21, 0x20);   // 主片 IRQ0→0x20
    outb(0xA1, 0x28);   // 从片 IRQ8→0x28
    // ICW3: 级联
    outb(0x21, 0x04);   // 主片 IRQ2 接从片
    outb(0xA1, 0x02);   // 从片接主片 IRQ2
    // ICW4: 8086模式 + 自动EOI关闭 + 缓冲模式关闭
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    // OCW1: 屏蔽所有中断
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

// === 键盘 (Set 1 扫描码→ASCII, 轮询模式) ===

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
    serial_init();
    pic_init();
    printk("JIAYAN KERNEL v4");
    printk_nl();
    printk("PIC OK | COM1 OK | SEED:828");
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
        否则 {
            __asm(0xF4);  // hlt — 等待下次中断/事件
        }
    }
}
