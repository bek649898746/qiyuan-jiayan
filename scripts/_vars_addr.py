# -*- coding: utf-8 -*-
import struct
def sec(f):
    data = open(f,'rb').read()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    nsec = struct.unpack_from('<H', data, pe+6)[0]
    opt = struct.unpack_from('<H', data, pe+20)[0]
    soff = pe+24+opt
    for i in range(nsec):
        off = soff + i*40
        name = data[off:off+8].rstrip(b'\x00').decode('ascii', errors='replace')
        vs, va, rs, ro = struct.unpack_from('<IIII', data, off+8)
        if name == '.data':
            return 0x400000 + va, ro
for f in ['v1.exe', 'v2.exe']:
    dva, ro = sec(f)
    print(f, 'data VMA=0x%x' % dva)
    # vars slot ~ str_tbl(1048576) + str_cnt(1)
    vars_addr = dva + 0x300 + 4 * (1048576 + 1)
    print(f, 'vars approx addr = 0x%x' % vars_addr)
