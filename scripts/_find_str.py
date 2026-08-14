# -*- coding: utf-8 -*-
"""找 STR 字符串节点类型"""
import io, sys, re
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
h = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c', 'rb').read().decode('utf-8', errors='replace')
lines = h.split('\n')
for i, l in enumerate(lines, 1):
    if re.search(r'STR\b.*Nd|tt\[ti\] == STR|case STR', l) and i < 3000:
        print('%5d: %s' % (i, l.strip()[:100]))
