# -*- coding: utf-8 -*-
import re, struct
def text(f):
    data = open(f,'rb').read()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    nsec = struct.unpack_from('<H', data, pe+6)[0]
    opt = struct.unpack_from('<H', data, pe+20)[0]
    soff = pe+24+opt
    for i in range(nsec):
        off = soff + i*40
        name = data[off:off+8].rstrip(b'\x00').decode('ascii', errors='replace')
        vs, va, rs, ro = struct.unpack_from('<IIII', data, off+8)
        if name == '.text':
            return bytes(data[ro:ro+rs])
for f in ['v1.exe', 'v2.exe']:
    t = text(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\\' + f)
    # case-1 static load: 8B 05 d0 followed by movsxd 48 63 C0 within 12 bytes
    cnt = 0
    for m in re.finditer(b'\x8b\x05', t):
        i = m.start()
        window = t[i+6:i+16]
        if b'\x48\x63\xc0' in window:
            cnt += 1
    # total movsxd
    total = len(re.findall(b'\x48\x63\xc0', t))
    print(f, 'case1-static-movsxd:', cnt, 'total movsxd:', total)
