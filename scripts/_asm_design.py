# -*- coding: utf-8 -*-
"""设计: asm 字符串编码器 (在 __asm 前插入)"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
print('设计: asm_enc_string(const char*) — 解析 x86 汇编字符串 → 发射字节')
print('''
/* fix 2026-08-11 汇编本身: __asm("指令") 字符串汇编编码器
   支持内核常用指令 (bin_mode):
   - 无操作数: hlt(F4) nop(90) ret(C3) sti(FB) cli(FA) iretq(48 CF) int3(CC)
   - mov/add/sub/cmp/and/or/xor: op dst, src (寄存器/立即数)
   - push/pop: 寄存器
   - call/jmp: 相对地址 (占位)
   - movzx/movsx: 字节扩展
   语法: AT&T 风格 "mov %rax, %rbx" 或 Intel "mov rax, rbx" — 统一用简写
''')
