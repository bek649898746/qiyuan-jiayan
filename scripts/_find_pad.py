# -*- coding: utf-8 -*-
"""找 __pad0 定义"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
t = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c', 'rb').read().decode('utf-8', errors='replace')
lines = t.split('\n')
for i, l in enumerate(lines, 1):
    if '__pad0' in l or 'STACK_PAD_SIZE' in l:
        print('%5d: %s' % (i, l.strip()[:100]))
