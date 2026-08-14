# -*- coding: utf-8 -*-
"""Compare v1 -S vs v2 -S of the mirror source.
v1 -S = v2's source text; v2 -S = v3's source text.  Since v1==v3 (machine bytes),
the -S texts should be IDENTICAL if both compilers behave the same.  Any real
difference = the 2-cycle seed (a codegen decision that differs between v1 and v2).

Normalize cosmetic formatting differences so only MACHINE-CODE-relevant diffs remain:
  - [rbp+N] vs [rbp+-N]  (host uses %+d, mirror uses +%d) -> same bytes
  - '加64 r0, r11' vs '加64' (some branches spell operands, some don't) -> same bytes
  - [rip+N] offsets, .Lx labels
"""
import re, sys

def norm(path):
    raw = open(path, 'rb').read()
    txt = raw.decode('utf-8', errors='replace')
    lines = txt.splitlines()
    out = []
    for l in lines:
        l2 = re.sub(r'\[rip\+[0-9]+\]', '[rip+X]', l)
        l2 = re.sub(r'\.L[0-9]+', '.Lx', l2)
        l2 = l2.replace('rbp+-', 'rbp-').replace('rsp+-', 'rsp-')
        out.append(l2)
    return out

def main():
    a = norm('scratch_test/_sv1x.asm.asm')  # v1 -S
    b = norm('scratch_test/_sv2x.asm.asm')  # v2 -S
    print('v1 -S lines:', len(a), ' v2 -S lines:', len(b))
    i = 0
    diffs = []
    while i < min(len(a), len(b)):
        if a[i] != b[i]:
            diffs.append((i, a[i][:100], b[i][:100]))
            if len(diffs) >= 12:
                break
        i += 1
    print('first diff at line', diffs[0][0] if diffs else 'NONE')
    for d in diffs:
        print('v1:', d[1])
        print('v2:', d[2])
        print()

if __name__ == '__main__':
    main()
