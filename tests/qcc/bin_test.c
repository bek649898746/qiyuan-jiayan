// @EXPECTED exit:0
// Multiboot2 header + 裸机内核骨架 (bin 模式测试)
// 布局: header (前 64 字节) + _start 入口
static void __attribute__((section(".multiboot"))) mb_header(void) {
    asm_emit("", 0, 0, 0);
}

int _start(void) {
    // 写 VGA 显存 0xB8000: 'A' 绿色
    asm_emit("    mov $0xB8000, %rax\n", 0, 0, 0);
    asm_emit("    mov $0x0241, %rbx\n", 0, 0, 0);
    asm_emit("    mov %bx, (%rax)\n", 0, 0, 0);
    // 死循环
    asm_emit("    cli\n", 0, 0, 0);
    asm_emit("    hlt\n", 0, 0, 0);
    asm_emit("    jmp .\n", 0, 0, 0);
    return 0;
}
