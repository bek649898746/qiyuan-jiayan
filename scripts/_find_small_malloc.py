# -*- coding: utf-8 -*-
"""Find ALL malloc/calloc/realloc call sites in the mirror, focusing on small fixed sizes.
The 58-byte overflow block must come from a small allocation in codegen.
"""
import re
m = open('srclib_jiayan/qcc_work.jy','rb').read().decode('utf-8', errors='replace')
lines = m.splitlines()
for i, l in enumerate(lines, 1):
    s = l.strip()
    # malloc/calloc/realloc with small literal sizes
    for mm in re.finditer(r'(malloc|calloc|realloc)\(\s*(\d+)\s*[,)]', s):
        fn, sz = mm.group(1), int(mm.group(2))
        if sz <= 256:
            print(f'{i}: {fn}({sz}) :: {s[:110]}')
