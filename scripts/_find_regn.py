# -*- coding: utf-8 -*-
"""看镜像 regn 初始化和 str_row"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
t = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', 'rb').read().decode('utf-8-sig', errors='replace')
lines = t.split('\n')
for i, l in enumerate(lines, 1):
    if 'regn' in l or 'str_row' in l:
        print('%5d: %s' % (i, l.strip()[:100]))
