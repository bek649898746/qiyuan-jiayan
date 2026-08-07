// 甲言裸机内核 - 纯 __asm_byte (手写机器码, 无 codegen RIP 相对)
// 引导跳转后直接执行: 写 VGA 'A' + 死循环
空 _start(空) {
    __asm_byte(0x48); __asm_byte(0xB8);  // movabs $0xB8000, %rax
    __asm_byte(0x00); __asm_byte(0x80); __asm_byte(0x0B); __asm_byte(0x00);
    __asm_byte(0x00); __asm_byte(0x00); __asm_byte(0x00); __asm_byte(0x00);
    __asm_byte(0x66); __asm_byte(0xC7); __asm_byte(0x00);  // movw $0x241, (%rax)
    __asm_byte(0x41); __asm_byte(0x02);
    __asm_byte(0xFA);  // cli
    __asm_byte(0xF4);  // hlt
    __asm_byte(0xEB); __asm_byte(0xFE);  // jmp .
    返 0;
}
