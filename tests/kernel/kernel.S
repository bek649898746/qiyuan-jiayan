/* 甲言内核 - GCC 参考 (纯汇编 .text, 与甲言 -bin 字节完全对齐)
 * 编译: gcc -c kernel.S -o kernel.o ; objcopy -O binary --only-section=.text kernel.o kernel.bin
 * 甲言:  qcc_x86.exe -bin kernel.c -o kernel.bin
 * 源码 (汇编版) 与 甲言版 (C + __asm_byte) 产出相同字节
 */
.section .text
.global _start
_start:
    /* Multiboot1 header (12B) */
    .byte 0x02, 0xB0, 0xAD, 0x1B    /* magic 0x1BADB002 */
    .byte 0x00, 0x00, 0x00, 0x00    /* flags = 0 */
    .byte 0xFE, 0x4F, 0x52, 0xE4    /* checksum */
    /* VGA 'A' */
    .byte 0x66, 0xC7, 0x05, 0x00, 0x80, 0x0B, 0x00, 0x41, 0x02
    /* cli; hlt; jmp . */
    .byte 0xFA, 0xF4, 0xEB, 0xFE
