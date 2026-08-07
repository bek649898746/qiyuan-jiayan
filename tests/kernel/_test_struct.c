// struct 字段访问测试
typedef struct { 整 x; 整 y; } Point;

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

空 test_struct(整 com1, 整 val) {
    Point p;
    p.x = val;
    p.y = val + 1;
    若 (p.x == 3) { serial_puts(com1, "X=3\n"); }
    若 (p.y == 4) { serial_puts(com1, "Y=4\n"); }
}

空 _start(空) {
    整 c = 0x3F8;
    serial_init(c);
    test_struct(c, 3);
    serial_puts(c, "STRUCT OK\n");
    循环 (1) { __asm(0xF4); }
}
