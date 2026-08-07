// 甲言裸机内核 - 纯 VGA 写 + 手写死循环 (无循环语句)
空 _start(空) {
    *(无 短*)0xB8000 = 0x0241;
    __asm_byte(0xFA);  // cli
    __asm_byte(0xF4);  // hlt
    __asm_byte(0xEB);  // jmp .
    __asm_byte(0xFE);
}
