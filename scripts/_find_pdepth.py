# -*- coding: utf-8 -*-
"""搜镜像参数注册: 字 ** 的 p_esz (ptr_depth 处理)"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
t = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', 'rb').read().decode('utf-8-sig', errors='replace')
lines = t.split('\n')
for i, l in enumerate(lines, 1):
    if 'ptr_depth' in l or ('var_param' in l and 'pesz' in l) or ('字 **' in l and 'var_param' in l):
        print('%5d: %s' % (i, l.strip()[:110]))
