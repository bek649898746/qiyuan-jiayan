# -*- coding: utf-8 -*-
"""Find ALL divergence points between v1 -S and v2 -S of the mirror.
v1 -S = v2's text, v2 -S = v3's text. v1==v3 so any diff = 2-cycle seed.
Groups consecutive diff lines into blocks.
"""
import re

def norm(path):
    raw = open(path,'rb').read()
    txt = raw.decode('utf-8', errors='replace')
    lines = txt.splitlines()
    out = []
    for l in lines:
        l2 = re.sub(r'\[rip\+[0-9]+\]', '[rip+X]', l)
        l2 = re.sub(r'\.L[0-9]+', '.Lx', l2)
        l2 = l2.replace('rbp+-', 'rbp-').replace('rsp+-', 'rsp-')
        out.append(l2)
    return out

a = norm('scratch_test/_sv1n2.asm.asm')  # v1 -S
b = norm('scratch_test/_sv2n2.asm.asm')  # v2 -S
print('v1 -S lines:', len(a), ' v2 -S lines:', len(b))

# collect diff blocks
blocks = []
cur = None
for i in range(min(len(a), len(b))):
    if a[i] != b[i]:
        if cur is None:
            cur = [i, i]
        else:
            cur[1] = i
    else:
        if cur is not None and i - cur[1] > 3:
            blocks.append(cur)
            cur = None
if cur is not None:
    blocks.append(cur)

print('diff blocks:', len(blocks))
for blk in blocks[:30]:
    print(f'--- block lines {blk[0]}-{blk[1]} ({blk[1]-blk[0]+1} lines) ---')
    for j in range(blk[0], min(blk[1]+1, blk[0]+4)):
        print('  v1:', a[j][:80])
        print('  v2:', b[j][:80])
