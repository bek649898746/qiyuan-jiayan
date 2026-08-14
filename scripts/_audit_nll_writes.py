# -*- coding: utf-8 -*-
"""Audit all writes to ndbl/nll/nuns in the mirror, flag non-Nd indices."""
import re
m = open('srclib_jiayan/qcc_work.jy','rb').read().decode('utf-8', errors='replace')
lines = m.splitlines()
pat = re.compile(r'(ndbl|nll|nuns)\[([^\]]+)\]\s*=\s*1')
for i, l in enumerate(lines, 1):
    for mm in pat.finditer(l):
        arr, idx = mm.group(1), mm.group(2).strip()
        # flag indices that are not plain 'n' (Nd result) or a simple var
        if idx not in ('n', 'n0[n]', 'n1[n]', 'c', 'a', 'm'):
            print(f'{i}: {arr}[{idx}] = 1   :: {l.strip()[:100]}')
