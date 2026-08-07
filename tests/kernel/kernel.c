// 甲言裸机内核 - 手写 VGA 写 + codegen 死循环
空 _start(空) {
    // 手写: movabs $0xB8000,%rax; movw $0x241,(%rax); cli; hlt; jmp .
    __asm_byte(0x48); __asm_byte(0xB8);
    __asm_byte(0x00); __asm_byte(0x80); __asm_byte(0x0B); __asm_byte(0x00);
    __asm_byte(0x00); __asm_byte(0x00); __asm_byte(0x00); __asm_byte(0x00);
    __asm_byte(0x66); __asm_byte(0xC7); __asm_byte(0x00);
    __asm_byte(0x41); __asm_byte(0x02);
    __asm_byte(0xFA); __asm_byte(0xF4); __asm_byte(0xEB); __asm_byte(0xFE);
    返 0;
}
