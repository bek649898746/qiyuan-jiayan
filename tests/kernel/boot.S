/* 甲言内核 - 引导段 (32→64 位切换, 汇编参考)
 * 汇编: as --32 boot.S -o boot.o ; objcopy -O binary -j .text boot.o boot.bin
 * 这是内核的"启动引导"部分 (任何 x86_64 内核必备, Linux head_32.S 同理)
 * 内核业务逻辑 (VGA/键盘/输出) 用甲言写
 */
.section .text
.global _start
_start:
    /* Multiboot1 header (12 bytes) */
    .long 0x1BADB002
    .long 0x0
    .long -0x1BADB002

    /* 32 位保护模式入口 (Multiboot1 从这里进入) */
    cli
    /* 1. 载入 GDT */
    lgdt (gdtdesc)
    /* 2. 启用 PAE (CR4.PAE) */
    movl %cr4, %eax
    orl  $0x20, %eax
    movl %eax, %cr4
    /* 3. 初始化 identity-map 页表 (2MB 大页, 覆盖前 1GB) */
    movl $pd, %edi
    movl $0x83, %esi            /* present | writable | PS (2MB page) */
    movl $512, %ecx             /* 512 个大页 = 1GB */
1:  movl %esi, (%edi)
    addl $0x200000, %esi
    addl $8, %edi
    loop 1b
    movl $pdpt, %eax
    orl  $3, %eax
    movl %eax, pml4
    movl $pd, %eax
    orl  $3, %eax
    movl %eax, pdpt
    /* 4. 载入页表基址 (CR3) */
    movl $pml4, %eax
    movl %eax, %cr3
    /* 5. 启用长模式 (EFER.LME) */
    movl $0xC0000080, %ecx
    rdmsr
    orl  $0x100, %eax
    wrmsr
    /* 6. 启用分页 (CR0.PG) → 进入长模式 */
    movl %cr0, %eax
    orl  $0x80000000, %eax
    movl %eax, %cr0
    /* 7. 远跳到 64 位代码段 */
    ljmp $0x08, $long_mode

.code64
long_mode:
    /* 64 位: 设栈 */
    movabs $0x200000, %rsp
    /* 跳到内核主体 (甲言 codegen 的 _start) */
    jmp kernel_main

    /* GDT (64 位代码段 @ 0x08) */
.align 8
gdt:
    .quad 0x0000000000000000       /* null */
    .quad 0x0020980000000000       /* code64: L=1, DPL=0, P=1 */
gdtdesc:
    .word (gdtdesc - gdt - 1)
    .long gdt

    /* 页表 (identity map 前 1GB, 2MB 大页) */
.align 4096
pml4: .skip 4096
pdpt: .skip 4096
pd:   .skip 4096
