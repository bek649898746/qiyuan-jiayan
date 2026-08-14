# -*- coding: utf-8 -*-
"""Compare the -S .asm.asm files for MACHINE-CODE differences.
The .asm.asm text lines, when assembled, should produce identical bytes.
We approximate by comparing instruction mnemonics AFTER normalizing
the [rsp+N] vs [rsp+N] text formatting differences that are cosmetic.

Key: host uses '%+d' (so -4416 prints as -4416, +8 as +8)
     mirror uses '+%d' (so -4416 prints as +-4416)
These are the SAME machine code.  Normalize both to a canonical form.
"""
import re

def norm_lines(path):
    raw = open(path, 'rb').read()
    txt = raw.decode('utf-8', errors='replace')
    lines = txt.splitlines()
    out = []
    for l in lines:
        l2 = re.sub(r'\[rip\+[0-9]+\]', '[rip+X]', l)
        l2 = re.sub(r'\.L[0-9]+', '.Lx', l2)
        # normalize [rbp+-N] -> [rbp-N], [rbp+N] -> [rbp+N]
        l2 = l2.replace('rbp+-', 'rbp-').replace('rsp+-', 'rsp-')
        out.append(l2)
    return out

a = norm_lines('scratch_test/_sv1f.asm.asm')  # host -S (v1 src)
b = norm_lines('scratch_test/_sv3g.asm.asm')  # v2 -S (v3 src)
print('host -S lines:', len(a), ' v2 -S lines:', len(b))
i = 0
diffs = []
while i < min(len(a), len(b)):
    if a[i] != b[i]:
        diffs.append((i, a[i][:100], b[i][:100]))
        if len(diffs) >= 10:
            break
    i += 1
print('first normalized diff at line', diffs[0][0] if diffs else 'NONE')
for d in diffs:
    print('A(host):', d[1])
    print('B(v2)  :', d[2])
    print()
