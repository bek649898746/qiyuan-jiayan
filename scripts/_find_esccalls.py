# -*- coding: utf-8 -*-
import re
m = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', encoding='utf-8').read()
lines = m.split(chr(10))
for i, l in enumerate(lines):
    if 'var_esz(' in l and '静 整 var_esz' not in l and 'static int var_esz' not in l:
        print((i+1), '|', l.strip()[:120])
