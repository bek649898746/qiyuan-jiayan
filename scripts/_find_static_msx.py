# -*- coding: utf-8 -*-
lines = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\scratch_test\_s_v1.asm.asm', encoding='utf-8', errors='replace').read().splitlines()
# find 取静32 (static int load) immediately followed by 符号扩展 (movsxd)
cur = None
hits = []
for i in range(len(lines)-1):
    l = lines[i]
    if l and not l.startswith(' ') and not l.startswith('.L') and l.endswith(':'):
        cur = l
    if '取静32' in l and '符号扩展' in lines[i+1]:
        hits.append((i, cur))
print('static-load+movsxd count:', len(hits))
for i, fn in hits:
    print('  line %d in %s' % (i, fn))
