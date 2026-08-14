# -*- coding: utf-8 -*-
import struct

d = open('v1b3.exe', 'rb').read()
pe = struct.unpack_from('<I', d, 0x3c)[0]
opt = pe + 24
so = opt + 240
vs, va, rs, ro = struct.unpack_from('<IIII', d, so + 8)
text = d[ro:ro + rs]
hits = []
for i in range(len(text) - 8):
    if text[i:i+3] == b'\x44\x01\xd0' and text[i+3] == 0x89:
        hits.append(i)
lines = ['set pagination off', 'set confirm off']
for h in hits:
    rt = 0x400000 + 0x1000 + h
    lines.append('break *0x%x' % rt)
    lines.append('commands')
    lines.append('  silent')
    lines.append('  printf "MALLOC size=%ld ctr=%#lx\\n", $rcx, *(int*)0x4f2000')
    lines.append('  continue')
    lines.append('end')
lines.append('run srclib_jiayan/qcc_work.jy -o scratch_test/_gdb_watch_out.exe')
lines.append('quit')
open(r'scratch_test\_gdb_allmalloc.txt', 'w').write('\n'.join(lines))
print('wrote', len(hits), 'breakpoints')
