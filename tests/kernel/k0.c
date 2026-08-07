// 甲言裸机内核骨架 - VGA 输出测试
// 用指针直接写 VGA 显存 (不需要裸指令)
volatile unsigned short *vga = (volatile unsigned short*)0xB8000;

void vga_put(char c) {
    *vga = (unsigned short)(0x0200 | (unsigned char)c);  // 绿色字符
    vga++;
}

int _start(void) {
    vga_put('A');
    vga_put('B');
    vga_put('C');
    // 死循环 (spin)
    while (1) { }
    return 0;
}
