# -*- coding: utf-8 -*-
"""定位 0xa2 字段."""
import struct, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
a = open(r'scratch_test/_h2t.asm', 'rb').read()
b = open(r'scratch_test/_h2t_a.exe', 'rb').read()
pe = struct.unpack_from('<I', a, 0x3c)[0]
print('PE offset:', hex(pe), 'opt:', hex(pe + 24))
for off in [0x9c, 0xa0, 0xa2, 0xa4, 0xa8, 0xac]:
    va = struct.unpack_from('<H', a, off)[0]
    vb = struct.unpack_from('<H', b, off)[0]
    mark = '  <-- DIFF' if va != vb else ''
    print('0x%02x: 直发 %04x asm %04x%s' % (off, va, vb, mark))
# 可选头 DllCharacteristics @opt+0x48 = 0xa0
print('DllChars @0xa0:', hex(struct.unpack_from('<H', a, 0xa0)[0]), hex(struct.unpack_from('<H', b, 0xa0)[0]))
# 0xa2 = opt+0x4A
