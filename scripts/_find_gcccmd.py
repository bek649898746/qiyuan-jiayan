# -*- coding: utf-8 -*-
"""查 build.ps1 的 gcc 命令 (是否 -DCODE_BUF_CAP)"""
import io, sys, re
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
t = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\scripts\build.ps1', 'rb').read().decode('utf-8-sig', errors='replace')
lines = t.split('\n')
for i, l in enumerate(lines, 1):
    if 'gcc' in l or 'qcc_x86.c' in l:
        print('%5d: %s' % (i, l.strip()[:130]))
