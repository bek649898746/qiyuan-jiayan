# -*- coding: utf-8 -*-
"""Audit ALL tt[...]=NK sites and whether they clear tll/tuns/tll_hi."""
import re
m = open('srclib_jiayan/qcc_work.jy','rb').read().decode('utf-8', errors='replace')
lines = m.splitlines()
for i, l in enumerate(lines, 1):
    if re.search(r'tt\[[^\]]+\]\s*=\s*NK', l):
        cleared = ('tll[ti] = 0' in l) or ('tll[ti]=0' in l)
        tuns_cleared = ('tuns[ti] = 0' in l) or ('tuns[ti]=0' in l)
        print(f'{i}: NK set | tll_cleared={cleared} tuns_cleared={tuns_cleared} :: {l.strip()[:130]}')
