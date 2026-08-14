# -*- coding: utf-8 -*-
import re
m = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', encoding='utf-8').read()
lines = m.split(chr(10))
for i, l in enumerate(lines):
    if re.search(r'\btt\b', l) and ('整 tt' in l or 'tt[' in l) and 'tt[' in l and 'define' not in l:
        print((i+1), '|', l.strip()[:150])
