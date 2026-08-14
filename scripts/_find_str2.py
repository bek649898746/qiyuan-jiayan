# -*- coding: utf-8 -*-
"""找字符串节点类型 (STR) 和 str_tbl id 获取"""
import io, sys, re
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
h = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c', 'rb').read().decode('utf-8', errors='replace')
lines = h.split('\n')
for i, l in enumerate(lines, 1):
    if ('STR' in l and ('nt[' in l or '== 4' in l)) or ('str_tbl' in l and 'str_idx' in l and 'nv[' in l):
        print('%5d: %s' % (i, l.strip()[:100]))
