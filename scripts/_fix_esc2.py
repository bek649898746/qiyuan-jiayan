# -*- coding: utf-8 -*-
"""修复: C 源码里的 '\\\\t' (字面4反斜杠) → '\\t' (C 转义制表符)
Python 读文件: '\\\\t' 是 4 个字符 \\ t. C 要的是 '\t' (反斜杠+t 2字符)"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

p = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c'
t = open(p, 'rb').read().decode('utf-8', errors='replace')

# Python 字符串: '\\\\t' = 4 反斜杠 + t (实际文件里的字面). 目标: '\\t' = 2 反斜杠 + t
old = "'\\\\t'"  # Python 字面: 4 反斜杠 t
new = "'\\t'"    # Python 字面: 2 反斜杠 t (C 源里就是 '\t' 字面转义)
n = t.count(old)
t = t.replace(old, new)
print('替换 %d 处' % n)

# 删未用 s64
old2 = 'int d64 = dst[0] == \'r\', s64 = src[0] == \'r\';'
new2 = 'int d64 = dst[0] == \'r\';'
n2 = t.count(old2)
t = t.replace(old2, new2)
print('s64 删 %d 处' % n2)

open(p, 'wb').write(t.encode('utf-8'))
print('[OK] 修复完成')
