# -*- coding: utf-8 -*-
"""找宿主 str_cnt 守卫"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
t = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c', 'rb').read().decode('utf-8', errors='replace')
lines = t.split('\n')
for i, l in enumerate(lines, 1):
    if 'str_cnt >= 1024' in l or 'str_cnt >= 2048' in l:
        print('%5d: %s' % (i, l.strip()[:100]))
