# -*- coding: utf-8 -*-
"""找镜像 str_tbl 声明"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
t = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', 'rb').read().decode('utf-8-sig', errors='replace')
lines = t.split('\n')
for i, l in enumerate(lines, 1):
    if 'str_tbl[' in l:
        print('%5d: %s' % (i, l.strip()[:100]))
