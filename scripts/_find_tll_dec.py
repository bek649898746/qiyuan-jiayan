# -*- coding: utf-8 -*-
import re
m = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', encoding='utf-8').read()
lines = m.split(chr(10))
for i, l in enumerate(lines):
    if re.search(r'tll', l) and ('静' in l or '[' in l) and i < 1900:
        print((i+1), '|', l.strip()[:160])
print('=== host ===')
h = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c', 'rb').read().decode('utf-8', errors='replace')
hl = h.split(chr(10))
for i, l in enumerate(hl):
    if re.search(r'tll', l) and ('static' in l or '[' in l or 'memset' in l) and i < 2000:
        print((i+1), '|', l.strip()[:160])
