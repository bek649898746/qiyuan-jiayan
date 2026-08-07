// 甲言裸机内核 - VGA 'A' 输出 (qemu 验证通过)
// 编译: qcc_x86.exe -bin kernel.c -o kernel.bin
// 引导: boot_pm.S (Multiboot1 + 32→64 长模式) 拼接
// 验证: VGA 0xB8000 = 0x41 0x02 (绿色 'A')
空 _start(空) {
    // movabs $0xB8000, %rax; movw $0x0241, (%rax); cli; hlt; jmp .
    __asm_byte(0x48); __asm_byte(0xB8);
    __asm_byte(0x00); __asm_byte(0x80); __asm_byte(0x0B); __asm_byte(0x00);
    __asm_byte(0x00); __asm_byte(0x00); __asm_byte(0x00); __asm_byte(0x00);
    __asm_byte(0x66); __asm_byte(0xC7); __asm_byte(0x00);
    __asm_byte(0x41); __asm_byte(0x02);
    __asm_byte(0xFA); __asm_byte(0xF4); __asm_byte(0xEB); __asm_byte(0xFE);
    返 0;
}
