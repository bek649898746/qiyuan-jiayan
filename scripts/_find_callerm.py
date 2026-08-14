# -*- coding: utf-8 -*-
"""找镜像谁调用 fn_macro_expand_to"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
t = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', 'rb').read().decode('utf-8-sig', errors='replace')
lines = t.split('\n')
for i, l in enumerate(lines, 1):
    if 'fn_macro_expand_to' in l and 'static' not in l and 'void' not in l and '{ /*' not in l:
        print('%5d: %s' % (i, l.strip()[:110]))
