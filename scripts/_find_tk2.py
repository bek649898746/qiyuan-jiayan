# -*- coding: utf-8 -*-
import re
m = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', encoding='utf-8').read()
lines = m.split(chr(10))
for i, l in enumerate(lines[:200]):
    if 'tk' in l and ('整' in l or '静' in l) and 'tk;' in l:
        print((i+1), '|', l.strip()[:140])
# search all declarations containing 'tk' as an identifier
for i, l in enumerate(lines):
    if re.search(r'\btk\b', l) and ('整 tk' in l or 'tk;' in l or 'tk,' in l):
        print((i+1), '|', l.strip()[:140])
