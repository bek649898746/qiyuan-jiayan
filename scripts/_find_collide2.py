# -*- coding: utf-8 -*-
"""Find name collisions between fn-local static arrays and global static arrays."""
import re

raw = open(r'srclib_jiayan\qcc_work.jy', 'rb').read()
txt = raw.decode('utf-8')
lines = txt.splitlines()

glob_arr = {}
local_arr = {}
depth = 0
for i, l in enumerate(lines):
    m = re.match(r'^\s*静\s+(?:无符号\s+)?(?:整|字|浮|长)\s+(\w+)\s*\[', l)
    if m:
        if depth == 0:
            glob_arr.setdefault(m.group(1), []).append(i + 1)
        else:
            local_arr.setdefault(m.group(1), []).append(i + 1)
    depth += l.count('{') - l.count('}')

print('global static arrays:', len(glob_arr))
print('fn-local static arrays (distinct names):', len(local_arr), 'total:', sum(len(v) for v in local_arr.values()))
collisions = [n for n in local_arr if n in glob_arr]
print('=== COLLISIONS ===')
for n in sorted(collisions):
    print(' ', n, 'global@', glob_arr[n], 'local@', local_arr[n])
