# -*- coding: utf-8 -*-
"""Compare asm_emit text between host and mirror to find desync points."""
import re

h = open('srclib/qcc_x86.c','rb').read().decode('gbk', errors='replace')
m = open('srclib_jiayan/qcc_work.jy','rb').read().decode('utf-8', errors='replace')

def emits(src):
    return re.findall(r'asm_emit\(\s*"([^"]*)"', src)

he = emits(h)
me = emits(m)
print('host emits:', len(he), ' mirror emits:', len(me))

hset = set(he)
mset = set(me)
only_h = [x for x in he if x not in mset]
only_m = [x for x in me if x not in hset]
print()
print('host-only emits:', len(only_h))
for x in only_h[:25]:
    print(' H:', repr(x))
print()
print('mirror-only emits:', len(only_m))
for x in only_m[:25]:
    print(' M:', repr(x))
