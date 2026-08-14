# -*- coding: utf-8 -*-
"""镜像 case-12 store 加 DBG-STORE 打印."""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
p = 'srclib_jiayan/qcc_work.jy'
lines = open(p, encoding='utf-8').read().split('\n')
for i, l in enumerate(lines):
    if '若 (nt[pnode] == 1) pe = var_esz' in l:
        print('found at', i + 1)
        lines.insert(i + 1, '                fprintf(stderr, "DBG-STORE: pnode=%d nt=%d pesz=%d\\n", pnode, nt[pnode], pesz[pnode]); /* TEMP */')
        break
open(p, 'w', encoding='utf-8').write('\n'.join(lines))
print('inserted')
