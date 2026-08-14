# -*- coding: utf-8 -*-
"""找宿主 fn_macro_expand_to 签名"""
import io, sys, re
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
t = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c', 'rb').read().decode('utf-8', errors='replace')
lines = t.split('\n')
for i, l in enumerate(lines, 1):
    if 'fn_macro_expand_to' in l and ('static' in l or 'void' in l):
        print('%5d: %s' % (i, l.strip()[:110]))
