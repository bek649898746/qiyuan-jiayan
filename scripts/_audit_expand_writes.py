# -*- coding: utf-8 -*-
"""Audit ALL writes to out[] in fn_macro_expand_to (L239-345) and flag unguarded ones."""
import re
m = open('srclib_jiayan/qcc_work.jy','rb').read().decode('utf-8', errors='replace')
lines = m.splitlines()
# fn_macro_expand_to body: L239-345
for i in range(239, 346):
    l = lines[i-1].strip()
    if 'out[(*o)++]' in l or 'out[(*o)]' in l or "out[(*o)" in l:
        guarded = ('*cap' in l) or ('(*o +' in l) or ('(*o+' in l)
        print(f'{i}: {"GUARDED" if guarded else "UNGUARDED"} :: {l[:110]}')
