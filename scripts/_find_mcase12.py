# -*- coding: utf-8 -*-
"""找镜像 case 12 (解引用) 的 el 判断"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
t = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', 'rb').read().decode('utf-8-sig', errors='replace')
lines = t.split('\n')
for i, l in enumerate(lines, 1):
    if 'case 12' in l or '12: { /* *ptr' in l:
        print('%5d: %s' % (i, l.strip()[:110]))
