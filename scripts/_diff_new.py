# -*- coding: utf-8 -*-
import re
def norm(f):
    lines = open(f, encoding='utf-8', errors='replace').read().splitlines()
    out = []
    for l in lines:
        l2 = re.sub(r'\[rip\+[0-9]+\]', '[rip+X]', l)
        out.append(l2)
    return out
a = norm(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\scratch_test\_sv1b.asm.asm')
b = norm(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\scratch_test\_sv2b.asm.asm')
print('lines:', len(a), len(b))
i = 0
while i < min(len(a), len(b)):
    if a[i] != b[i]:
        break
    i += 1
print('first diff at line', i)
# print context: v1 and v2
print('=== v1 ===')
for j in range(max(0,i-8), min(len(a), i+14)):
    print(j, a[j].encode('unicode_escape').decode()[:80])
print('=== v2 ===')
for j in range(max(0,i-8), min(len(b), i+14)):
    print(j, b[j].encode('unicode_escape').decode()[:80])
