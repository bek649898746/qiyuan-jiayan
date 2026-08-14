# -*- coding: utf-8 -*-
"""Compare asm_emit text between host and mirror using UTF-8 decode."""
import re

h = open('srclib/qcc_x86.c','rb').read().decode('utf-8', errors='replace')
m = open('srclib_jiayan/qcc_work.jy','rb').read().decode('utf-8', errors='replace')

def emits(src):
    return re.findall(r'asm_emit\(\s*"([^"]*)"', src)

he_all = emits(h)
me_all = emits(m)
he = set(he_all)
me = set(me_all)
only_h = [x for x in he_all if x not in me]
only_m = [x for x in me_all if x not in he]
print('host emits:', len(he_all), ' mirror emits:', len(me_all))
print('host-only:', len(only_h))
for x in only_h[:25]:
    print(' H:', repr(x))
print()
print('mirror-only:', len(only_m))
for x in only_m[:25]:
    print(' M:', repr(x))
