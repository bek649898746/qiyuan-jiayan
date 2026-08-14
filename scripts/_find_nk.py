# -*- coding: utf-8 -*-
"""Find ALL places that set tt[...] = NK in the mirror."""
import re
m = open('srclib_jiayan/qcc_work.jy','rb').read().decode('utf-8', errors='replace')
lines = m.splitlines()
for i, l in enumerate(lines, 1):
    if re.search(r'tt\[[^\]]+\]\s*=\s*NK', l):
        print(f'{i}: {l.strip()[:120]}')
