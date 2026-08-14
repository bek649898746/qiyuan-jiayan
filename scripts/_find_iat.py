# -*- coding: utf-8 -*-
import struct
for f in ['v1.exe', 'v2.exe']:
    data = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\\' + f, 'rb').read()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    nsec = struct.unpack_from('<H', data, pe+6)[0]
    opt = struct.unpack_from('<H', data, pe+20)[0]
    soff = pe + 24 + opt
    dla = None
    for i in range(nsec):
        off = soff + i*40
        name = data[off:off+8].rstrip(b'\x00').decode('ascii', errors='replace')
        vs, va, rs, ro = struct.unpack_from('<IIII', data, off+8)
        if name == '.data':
            dla = (0x400000 + va, ro)
    dva, dro = dla
    # import names: find "VirtualAlloc" in .data
    i = data.find(b'VirtualAlloc\x00')
    print(f, 'VirtualAlloc name at file 0x%x' % i)
    # IAT is at .data+8 (kernel32), slot 4 = VirtualAlloc
    print(f, 'data VMA=0x%x' % dva, 'IAT1 slot4 VA=0x%x' % (dva + 8 + 4*8))
