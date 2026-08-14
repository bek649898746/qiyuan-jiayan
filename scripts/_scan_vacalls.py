# -*- coding: utf-8 -*-
import struct
data = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\v1.exe', 'rb').read()
pe = struct.unpack_from('<I', data, 0x3c)[0]
nsec = struct.unpack_from('<H', data, pe+6)[0]
opt = struct.unpack_from('<H', data, pe+20)[0]
soff = pe + 24 + opt
for i in range(nsec):
    off = soff + i*40
    name = data[off:off+8].rstrip(b'\x00')
    vs, va, rs, ro = struct.unpack_from('<IIII', data, off+8)
    if name == b'.text':
        t = data[ro:ro+rs]
        tbase = va  # RVA of text
        sites = []
        i = 0
        while i < len(t) - 6:
            if t[i] == 0xff and t[i+1] == 0x15:
                disp = struct.unpack_from('<i', t, i+2)[0]
                target = tbase + i + 6 + disp
                if target == 0x4f2028:
                    sites.append(tbase + i + 6)  # next instruction RVA
            i += 1
        print('text RVA=0x%x, call sites targeting 0x4f2028:' % tbase)
        print('  return RVAs:', [hex(s) for s in sites])
