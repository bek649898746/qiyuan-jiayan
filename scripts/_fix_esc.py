# -*- coding: utf-8 -*-
"""修复编码器的转义问题: '\\t' → '\t' (Python 转义)"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

p = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c'
t = open(p, 'rb').read().decode('utf-8', errors='replace')

# Python 字符串里 '\\t' 表示字面 \t (反斜杠+t). 在 C 里应写成 '\t' (制表符).
# 所以把 C 源码里的 '\\t' (两个字符: \t) 改成 '\t' (反斜杠+t = C 转义制表符)
t = t.replace("'\\\\t'", "'\\t'")

# 删掉未用的 s64
t = t.replace('int d64 = dst[0] == \'r\', s64 = src[0] == \'r\';', 'int d64 = dst[0] == \'r\';')

open(p, 'wb').write(t.encode('utf-8'))
print('[OK] 转义修复')
# 验证
t2 = open(p, 'rb').read().decode('utf-8', errors='replace')
import re
for i, l in enumerate(t2.split('\n'), 1):
    if '\\\\t' in l:
        print('残留 %d: %s' % (i, l.strip()[:80]))
