# -*- coding: utf-8 -*-
"""查 asm volatile / 字符串汇编支持"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
h = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c', 'rb').read().decode('utf-8', errors='replace')
for kw in ['asm volatile', 'volatile(', '__asm__', 'asm(', '"mov', 'asm_string', 'asm_str']:
    print('%s: %d 次' % (kw, h.count(kw)))
