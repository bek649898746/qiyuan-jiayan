# -*- coding: utf-8 -*-
"""找镜像 cg 函数签名"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
t = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', 'rb').read().decode('utf-8-sig', errors='replace')
lines = t.split('\n')
for i, l in enumerate(lines, 1):
    if 'cg(整 n)' in l or 'cg(整 n) {' in l or '静 空 cg' in l:
        print('%5d: %s' % (i, l.strip()[:80]))
