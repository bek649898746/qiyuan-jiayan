# -*- coding: utf-8 -*-
"""看宿主文件头前 100 字节 (hex)"""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
d = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib\qcc_x86.c', 'rb').read()
print('前 120 字节 hex:')
print(' '.join('%02X' % x for x in d[:120]))
print()
print('可打印:')
print(repr(d[:120]))
