# -*- coding: utf-8 -*-
import struct
data = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\v1.exe', 'rb').read()
pe = struct.unpack_from('<I', data, 0x3c)[0]
nsec = struct.unpack_from('<H', data, pe+6)[0]
opt = struct.unpack_from('<H', data, pe+20)[0]
soff = pe + 24 + opt
dva = None
for i in range(nsec):
    off = soff + i*40
    name = data[off:off+8].rstrip(b'\x00').decode('ascii', errors='replace')
    vs, va, rs, ro = struct.unpack_from('<IIII', data, off+8)
    if name == '.data':
        dva = 0x400000 + va
# static slot layout (镜像 55-60 行附近)
slot = 0
def reg(n, slots):
    global slot
    slot += slots
reg('code', 2)
reg('cp,cc', 2)
reg('asm_out', 2)
reg('asm_pass,asm_mode', 2)
reg('lc,epi,brk,cont', 4)
reg('__pad0', 0x300000//4)
reg('str_tbl', 2048*2048//4)
reg('str_cnt', 1)
reg('vars[4096] x 120B', 4096*30)
reg('var_static_kw? vcnt?', 2)  # host has var_static_kw, mirror doesn't; assume misc
# ... this is getting unreliable. Instead, search .data for ndbl/nll patterns.
# nll[262144] is a 1MB array of ints; after compile it has sparse non-zero entries.
# We can locate it by finding a 1MB region with sparse nonzero ints.
print('data VMA=0x%x' % dva)
# rough estimate: after vars, fvb/fve, misc, then ndbl/nll/nuns/pesz
# let's find via gdb instead - print the estimate here
# mirror order (55-60 + 1227-1264):
# str_tbl, str_cnt, vars, vcnt, stc_n, fdef_list, fdef_n, root_global, ...,
# fvb, fve, fvn, fr_start, fr_end, ..., ndbl, nll, nuns, pesz
# The token arrays (tt/tv/tuns/tll/tll_hi) are _va_alloc pointers.
print('approximate: search .data for nll via gdb')
