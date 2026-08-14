# -*- coding: utf-8 -*-
"""找镜像的 __asm 内建"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
t = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', 'rb').read().decode('utf-8-sig', errors='replace')
lines = t.split('\n')
for i, l in enumerate(lines, 1):
    if '__asm' in l and ('内建' in l or 'nt[fn]' in l or 'fname' in l):
        print('%5d: %s' % (i, l.strip()[:110]))
