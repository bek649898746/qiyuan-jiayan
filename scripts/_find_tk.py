# -*- coding: utf-8 -*-
import re
m = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', encoding='utf-8').read()
lines = m.split(chr(10))
# find declarations of tk: 整 tk, 静 整 tk, etc.
for i, l in enumerate(lines):
    if re.search(r'(^|[;{]\s*)(静\s+)?整\s+tk\b', l):
        print((i+1), '|', l.strip()[:140])
