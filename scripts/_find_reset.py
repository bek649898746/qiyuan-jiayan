# -*- coding: utf-8 -*-
m = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', encoding='utf-8').read()
lines = m.split(chr(10))
# find parse() reset section: memset calls
for i, l in enumerate(lines):
    if 'memset' in l and ('ASZ' in l or 'TS' in l or 'tll' in l or 'tuns' in l):
        print((i+1), '|', l.strip()[:140])
# find tll declaration
for i, l in enumerate(lines):
    if 'tll[' in l and ('静' in l or 'declare' in l):
        print((i+1), '|', l.strip()[:140])
