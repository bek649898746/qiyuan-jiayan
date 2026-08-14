# -*- coding: utf-8 -*-
import struct
for f in ['v1.exe', 'v2.exe']:
    data = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\\' + f, 'rb').read()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    nsec = struct.unpack_from('<H', data, pe+6)[0]
    opt = struct.unpack_from('<H', data, pe+20)[0]
    soff = pe+24+opt
    dva = None
    for i in range(nsec):
        off = soff + i*40
        name = data[off:off+8].rstrip(b'\x00').decode('ascii', errors='replace')
        vs, va, rs, ro = struct.unpack_from('<IIII', data, off+8)
        if name == '.data':
            dva = 0x400000 + va
    # slot calc:
    slot = 0
    slot += 2  # code (ptr 8B)
    slot += 2  # cp, cc
    slot += 2  # asm_out ptr
    slot += 2  # asm_pass, asm_mode
    slot += 4  # lc, epi, brk, cont
    slot += 0x300000//4  # __pad0
    slot += 2048*2048//4  # str_tbl
    slot += 1  # str_cnt
    vars_slot = slot
    vars_addr = dva + 0x300 + 4*vars_slot
    print(f, 'data VMA=0x%x vars_slot=%d vars_addr=0x%x' % (dva, vars_slot, vars_addr))
