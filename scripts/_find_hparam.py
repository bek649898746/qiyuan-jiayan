# -*- coding: utf-8 -*-
"""找宿主 var_param 调用的 pesz 参数"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
t = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c', 'rb').read().decode('utf-8', errors='replace')
lines = t.split('\n')
for i, l in enumerate(lines, 1):
    if 'var_param(tn[tk]' in l or 'var_param(' in l and 'pis_ptr' in l:
        print('%5d: %s' % (i, l.strip()[:110]))
