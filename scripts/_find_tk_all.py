# -*- coding: utf-8 -*-
import re
m = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', encoding='utf-8').read()
lines = m.split(chr(10))
# declarations: word tk as a declared identifier, not usage
pat = re.compile(r'\btk\b')
for i, l in enumerate(lines):
    # declaration-ish: type keyword before tk
    if re.search(r'(静\s+)?(整|字|无\s+字|常\s+字|双|空)\s+[^;]*\btk\b', l):
        print((i+1), '|', l.strip()[:150])
print('--- param decls ---')
for i, l in enumerate(lines):
    if re.search(r'\(\s*[^)]*\btk\b[^)]*\)', l) and ('整' in l or '字' in l):
        print((i+1), '|', l.strip()[:150])
