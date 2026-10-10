# -*- coding: utf-8 -*-
"""反汇编探针产物: 找 `call qword [rip+rel32]`（FF 15），算其目标地址，
   与导入表 IAT 的 RVA 对照（CreateThread 槽 8 ⟹ IAT_RVA + 8*8）"""
import struct, sys

p = sys.argv[1]
IAT_RVA = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x4a008
IMG_BASE = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x140000000

b = open(p, 'rb').read()
pe = struct.unpack_from('<I', b, 0x3C)[0]
nsec = struct.unpack_from('<H', b, pe + 6)[0]
optsz = struct.unpack_from('<H', b, pe + 20)[0]
opt = pe + 24
secs = []
so = opt + optsz
for i in range(nsec):
    o = so + i * 40
    name = b[o:o + 8].rstrip(b'\x00').decode('ascii', 'replace')
    vsz, va, rsz, ra = (struct.unpack_from('<I', b, o + 8)[0], struct.unpack_from('<I', b, o + 12)[0],
                        struct.unpack_from('<I', b, o + 16)[0], struct.unpack_from('<I', b, o + 20)[0])
    secs.append((name, va, vsz, ra, rsz))

def off2rva(off):
    for name, va, vsz, ra, rsz in secs:
        if ra <= off < ra + rsz:
            return va + (off - ra)
    return None

text = [s for s in secs if s[0] == '.text'][0]
name, va, vsz, ra, rsz = text
print('.text VA=%#x raw=%#x rsz=%d' % (va, ra, rsz))
print('IAT 起始 RVA = %#x ; 槽 n 的地址 = %#x + 8n' % (IAT_RVA, IAT_RVA))

hits = []
i = ra
end = ra + rsz - 6
while i < end:
    if b[i] == 0xFF and b[i + 1] == 0x15:
        rel = struct.unpack_from('<i', b, i + 2)[0]
        rva_of_next = off2rva(i + 6)
        if rva_of_next is not None:
            tgt = rva_of_next + rel
            slot = (tgt - IAT_RVA) // 8
            ok = (IAT_RVA <= tgt < IAT_RVA + 64 * 8) and ((tgt - IAT_RVA) % 8 == 0)
            hits.append((off2rva(i), tgt, slot if ok else None, ok))
        i += 6
    else:
        i += 1

print('FF 15（call [rip+rel32]）命中 %d 条；其中落在 IAT 区（前 64 槽）的 = %d 条'
      % (len(hits), sum(1 for h in hits if h[3])))
print('\n前 20 条:')
print('%-12s %-14s %-6s %s' % ('指令RVA', '目标RVA', '槽号', '判'))
for rva, tgt, slot, ok in hits[:20]:
    print('%-12s %-14s %-6s %s' % (hex(rva) if rva else '?', hex(tgt),
                                   slot if slot is not None else '-', 'IAT ✓' if ok else '**非 IAT**'))
# 统计越界
bad = [(r, t) for r, t, s, ok in hits if not ok]
print('\n非 IAT 目标 共 %d 条；样例: %s' % (len(bad), [hex(t) for _, t in bad[:8]]))
