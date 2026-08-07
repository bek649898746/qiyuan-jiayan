// 甲言内核 v6 — Gate 2 编译时内存布局 + 串口输出
// 包含 kernel_mem.h 定义物理地址空间
// 构建: qcc_x86_new.exe -bin tests/kernel/kernel_v6.c -o scratch_test/kernel_v6.bin

#include "kernel/kernel_mem.h"

// === 串口驱动 ===

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

// === VGA 输出 ===

空 vga_putc(整 *pos, 整 c) {
    *(无 短*)(0xB8000 + (*pos) * 2) = (无 短)(0x0200 | (无 短)c);
    *pos = *pos + 1;
}
空 vga_puts(整 *pos, 字 *s) { 循环 (*s != 0) { vga_putc(pos, *s); s++; } }
空 vga_nl(整 *pos) { *pos = ((*pos / 80) + 1) * 80; }

空 printk(整 *pos, 整 com1, 字 *s) { vga_puts(pos, s); serial_puts(com1, s); }
空 printk_nl(整 *pos, 整 com1) { vga_nl(pos); serial_puts(com1, "\n"); }

// === PIC ===

空 pic_init(空) {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xFF); outb(0xA1, 0xFF);
}

// === 入口 ===

空 _start(空) {
    整 pos = 0;
    整 com1 = 0x3F8;
    serial_init(com1);
    pic_init();

    printk(&pos, com1, "JIAYAN v6 GATE2");
    printk_nl(&pos, com1);
    printk(&pos, com1, "MEM LAYOUT | SEED:828");
    printk_nl(&pos, com1);

    // 验证内存常量（编译时计算）
    printk(&pos, com1, "DDR5:256GB AGENT:16MB+");
    printk_nl(&pos, com1);
    printk(&pos, com1, "HBM:80GB KVCACHE+WEIGHTS");
    printk_nl(&pos, com1);

    // 使用 MMIO 常量验证（不实际访问）
    整 apic = MMIO_APIC;  // 编译时常量
    printk(&pos, com1, "MMIO:APIC=NIC=NVMe=GPU");
    printk_nl(&pos, com1);

    循环 (1) { __asm(0xF4); }
}
