# -*- coding: utf-8 -*-
import re
m = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', encoding='utf-8').read()
lines = m.split(chr(10))
for i, l in enumerate(lines):
    if re.search(r'\b(tt|tv|tuns|tll|tll_hi|tn|tk)\[', l) and ('静' in l or '字' in l or '整' in l):
        print((i+1), '|', l.strip()[:160])
