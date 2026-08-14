# -*- coding: utf-8 -*-
"""Trace the mirror's file-scope static registration order to compute nll[]'s .data slot.
The statics get slots in REGISTRATION order (file-scope parse order).
We approximate by scanning top-level static declarations in source order.
"""
import re

raw = open(r'srclib_jiayan\qcc_work.jy', 'rb').read()
txt = raw.decode('utf-8')
lines = txt.splitlines()

# We need the FIRST function definition line to bound file-scope.
fn_re = re.compile(r'^\s*(?:静\s+)?(?:整|空|字|浮|构|无符号)\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(')
# scan for the first top-level function
first_fn = None
depth = 0
for i, l in enumerate(lines):
    if l.startswith('静 ') and fn_re.match(l) and ';' not in l.split('{')[0][-3:]:
        # crude: a function def has '{' at the end
        if l.rstrip().endswith('{'):
            first_fn = i
            break
print('first function at line', first_fn)

slots = 0
entries = []
i = 0
STACK_PAD = None
while i < first_fn:
    l = lines[i]
    # __pad0[STACK_PAD_SIZE]
    m = re.search(r'__pad0\[(\w+)\]', l)
    if m:
        v = m.group(1)
        n = int(v, 0) if v.isdigit() else (int(v) if v.isdigit() else 0)
        # STACK_PAD_SIZE is a define; look it up
        for j in range(first_fn):
            dm = re.match(r'#define\s+STACK_PAD_SIZE\s+(\d+)', lines[j])
            if dm:
                n = int(dm.group(1))
        slots += n
        entries.append(('__pad0', slots))
        i += 1
        continue
    # static array decl: 静 <type> name[...] — count slots
    m = re.search(r'静\s+(?:无符号\s+)?(?:整|字|浮|长)\s+(\w+)\s*\[([^\]]+)\]', l)
    if m and i < first_fn:
        name, dim = m.group(1), m.group(2)
        # evaluate dim if integer
        try:
            n = int(dim)
        except ValueError:
            n = None
        esz = 1 if '字' in l.split('静')[1][:6] else 4  # char vs int
        if n is not None:
            slots += n
            entries.append((name, slots, n, esz))
    i += 1

print('total file-scope slots approx:', slots)
for e in entries:
    print(e)
