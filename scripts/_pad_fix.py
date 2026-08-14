# -*- coding: utf-8 -*-
"""宿主 STACK_PAD_SIZE 0x160000 -> 0x300000 (对齐镜像)"""
import io, sys, os
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

p = 'srclib/qcc_x86.c'
d = open(p, 'rb').read()
n = d.count(b'STACK_PAD_SIZE 0x160000')
d = d.replace(b'STACK_PAD_SIZE 0x160000', b'STACK_PAD_SIZE 0x300000')
open(p, 'wb').write(d)
print('宿主 STACK_PAD_SIZE 0x160000->0x300000 (%d 处)' % n)
