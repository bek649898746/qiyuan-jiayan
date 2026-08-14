# -*- coding: utf-8 -*-
"""Find string-buffer writes in fn_macro_expand_to / lexer that lack bounds guards."""
import re
m = open('srclib_jiayan/qcc_work.jy','rb').read().decode('utf-8', errors='replace')
lines = m.splitlines()
# look for tmp[ti2++] = ... WITHOUT a guard on the same line
for i, l in enumerate(lines, 1):
    if 'tmp[ti2++]' in l and 'ti2 <' not in l and 'ti2<' not in l:
        print(f'{i}: {l.strip()[:110]}')
