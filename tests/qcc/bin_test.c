// @EXPECTED exit:nonzero
// Multiboot2 header + 裸机内核骨架 (bin 模式测试)
// 布局: header (前 64 字节) + _start 入口
// 注: 裸机内核写 VGA 0xB8000 + cli/hlt, 在 Windows 用户态运行必然 0xC0000005 且不返回 —
//     编译覆盖保留 (H1==H2), 运行崩溃=预期 (fix 2026-08-09: 原 @EXPECTED exit:0 导致 CI 行为断言失败)
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
