# -*- coding: utf-8 -*-
"""摸底: 当前汇编相关能力 (-S模式/汇编器/内联汇编)"""
import io, sys, re
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

h = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c', 'rb').read().decode('utf-8', errors='replace')
lines = h.split('\n')

print('=== 汇编模式/关键字 ===')
for i, l in enumerate(lines, 1):
    if re.search(r'asm_mode|-S\b|"asm"|内联汇编|__asm|emit_asm|dump_asm|汇编输出', l):
        print('%5d: %s' % (i, l.strip()[:110]))
