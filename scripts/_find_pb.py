# -*- coding: utf-8 -*-
import re
for f, pat in [(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c', r'parse_base\s*='),
               (r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', r'parse_base\s*=')]:
    src = open(f, 'rb').read().decode('utf-8', errors='replace')
    lines = src.split(chr(10))
    print('===', f.split('\\')[-1], '===')
    for i, l in enumerate(lines):
        if re.search(pat, l):
            print(' ', i+1, '|', l.strip()[:100])
