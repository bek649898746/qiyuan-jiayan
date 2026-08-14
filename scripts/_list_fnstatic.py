# -*- coding: utf-8 -*-
import re
m = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', encoding='utf-8').read()
lines = m.split(chr(10))
# collect ALL static declarations (global + local) with their names
decls = []
for i, l in enumerate(lines):
    mm = re.search(r'静\s+(?:整|字|无\s+字|双)\s+(\w+)\s*([;,=\[])', l)
    if mm:
        decls.append((i+1, mm.group(1), l.strip()[:80]))
# find names appearing more than once (potential conflict)
from collections import Counter
c = Counter(nm for _, nm, _ in decls)
print('static var decls:', len(decls), 'unique names:', len(c))
print('--- names with >1 decl ---')
for nm, cnt in c.most_common():
    if cnt > 1:
        print(' ', nm, cnt)
        for i, n, l in decls:
            if n == nm:
                print('    line', i, '|', l)
