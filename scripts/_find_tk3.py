# -*- coding: utf-8 -*-
import re
m = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', encoding='utf-8').read()
lines = m.split(chr(10))
# any line that declares a variable named tk (整 tk / 静 整 tk / 字 tk etc.)
for i, l in enumerate(lines):
    if re.search(r'(^|[;{]\s*)(静\s+)?(整|字|无\s+字|常\s+字|双|空\s*\*)\s+tk\s*([,;=])', l):
        print((i+1), '|', l.strip()[:140])
