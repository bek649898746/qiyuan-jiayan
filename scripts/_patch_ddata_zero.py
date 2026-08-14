# -*- coding: utf-8 -*-
"""Byte-safe: add memset after each ddata calloc in GBK+UTF-8 host."""
p = 'srclib/qcc_x86.c'
data = open(p, 'rb').read()

old = b'uint8_t *ddata = (uint8_t*)calloc(stc_n ? stc_n : 1, 4);'
new = (b'uint8_t *ddata = (uint8_t*)calloc(stc_n ? stc_n : 1, 4);\r\n'
       b'    if (ddata) memset(ddata, 0, (stc_n ? stc_n : 1) * 4); /* fix 2026-08-12 UB: calloc bump no-zero -> uninit globals garbage */')

n = data.count(old)
print('ddata calloc count:', n)
if n == 3:
    data = data.replace(old, new)
    open(p, 'wb').write(data)
    print('patched all 3')
else:
    print('expected 3, got', n)
