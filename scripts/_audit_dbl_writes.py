# -*- coding: utf-8 -*-
"""Audit ALL ndbl[] writes - flag non-obvious indices."""
import re
m = open('srclib_jiayan/qcc_work.jy','rb').read().decode('utf-8', errors='replace')
lines = m.splitlines()
pat = re.compile(r'ndbl\[([^\]]+)\]')
seen = set()
for i, l in enumerate(lines, 1):
    for mm in pat.finditer(l):
        idx = mm.group(1).strip()
        if idx not in ('n', 'n0[n]', 'n1[n]', 'm', 'c', 'a', 'ce') and idx not in seen:
            seen.add(idx)
            print(f'{i}: ndbl[{idx}] :: {l.strip()[:100]}')
