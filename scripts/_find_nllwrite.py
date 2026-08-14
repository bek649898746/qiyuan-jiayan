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
t = text(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\v1.exe')
# C7 04 88 = mov dword [rax+rcx*4], imm32 ; C7 04 80 = [rax+rax*4] ; C7 44 XX = [reg+disp]
# general: C7 /r with mod=00 reg=000 rm=100 (SIB)
hits = []
for m in re.finditer(b'\xc7\x04', t):
    i = m.start()
    # byte after C7 04 is SIB
    sib = t[i+2]
    imm = struct.unpack_from('<i', t, i+3)[0]
    hits.append((i, sib, imm))
print('C7 04 (mov dword [sib], imm) count:', len(hits))
w1 = [h for h in hits if h[2] == 1]
print('...imm==1 count:', len(w1))
for i, sib, imm in w1[:15]:
    print('  text+0x%x sib=0x%02x (base=%d idx=%d) VA=0x%x' % (i, sib, sib&7, (sib>>3)&7, 0x400000+0x1000+i))
