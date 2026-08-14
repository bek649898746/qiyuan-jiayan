# -*- coding: utf-8 -*-
"""Find all places writing '"' (0x22) into a buffer."""
import re
m = open('srclib_jiayan/qcc_work.jy','rb').read().decode('utf-8', errors='replace')
lines = m.splitlines()
for i, l in enumerate(lines, 1):
    # write of '"' character: ['"'] or 0x22 or '\x22'
    if re.search(r"=\s*'\"'|=\s*0x22|= 34\b", l):
        print(f'{i}: {l.strip()[:110]}')
