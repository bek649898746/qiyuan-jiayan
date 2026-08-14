# -*- coding: utf-8 -*-
"""查 b4 是否存在 + 6049 是否在函数内"""
import io, sys, re
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
h = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c', 'rb').read().decode('utf-8', errors='replace')

print('b4 定义:', 'static void b4(' in h or 'void b4(' in h)
# 6049 在哪个函数内? 往前找函数签名
lines = h.split('\n')
for i in range(6048, 5500, -1):
    if re.match(r'^static \w+ \w+\(', lines[i]):
        print('6049 在函数内: %d: %s' % (i+1, lines[i].strip()[:60]))
        break
