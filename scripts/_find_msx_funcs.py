# -*- coding: utf-8 -*-
lines = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\scratch_test\_s_v1.asm.asm', encoding='utf-8', errors='replace').read().splitlines()
# _s_v1.asm.asm = V1 编译镜像的产物 = V2 的 asm
cur = None
hits = []
for i, l in enumerate(lines):
    if l and not l.startswith(' ') and not l.startswith('.L') and l.endswith(':'):
        cur = l
    elif '符号扩展' in l:
        hits.append((i, cur))
print('total movsxd in V2 asm:', len(hits))
# group by function
from collections import Counter
c = Counter(h for _, h in hits)
for fn, n in c.most_common(20):
    print('  %-20s %d' % (fn[:20], n))
print('--- first 12 positions ---')
for i, fn in hits[:12]:
    print('  line %d in %s' % (i, fn))
