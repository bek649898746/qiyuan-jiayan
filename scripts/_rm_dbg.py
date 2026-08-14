# -*- coding: utf-8 -*-
"""移除镜像调试打印."""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
p = 'srclib_jiayan/qcc_work.jy'
s = open(p, encoding='utf-8').read()

repl = [
    ('    若 (cpe) { pesz[ce] = cpe; fprintf(stderr, "DBG-CASTW: ce=%d cpe=%d readback=%d base=%d\\n", ce, cpe, pesz[ce], (int)pesz); } /* TEMP */',
     '    若 (cpe) pesz[ce] = cpe;'),
    ('                fprintf(stderr, "DBG-SP: %d base=%d\\n", pesz[pnode], (int)pesz); /* TEMP */\n', ''),
]
for old, new in repl:
    if old not in s:
        print('NOT FOUND:', old[:60])
    else:
        s = s.replace(old, new)
        print('REMOVED:', old[:60])
open(p, 'w', encoding='utf-8').write(s)
print('done')
