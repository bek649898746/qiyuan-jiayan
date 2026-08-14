# -*- coding: utf-8 -*-
"""List ALL malloc/calloc/realloc call sites with line numbers (mirror)."""
import re
m = open('srclib_jiayan/qcc_work.jy','rb').read().decode('utf-8', errors='replace')
lines = m.splitlines()
for i, l in enumerate(lines, 1):
    s = l.strip()
    if re.search(r'(malloc|calloc|realloc)\s*\(', s):
        print(f'{i}: {s[:120]}')
