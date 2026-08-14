# -*- coding: utf-8 -*-
"""清理 asm_zh 调试打印."""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
p = 'srclib/asm_zh.c'
s = open(p, encoding='utf-8').read()
repl = [
    ('fprintf(stderr,"DBG段: vsize=%X raw=%X\\n",exp_data_vsize,exp_data_raw);', ''),
    ('fprintf(stderr,"DBGpad: df=%X pos=%X raw=%X drend=%X\\n",df,pos,exp_data_raw,drend);', ''),
    ('fprintf(stderr,"DBGpad2: end=%X\\n",pos);', ''),
]
for old, new in repl:
    if old in s:
        s = s.replace(old, new)
        print('removed:', old[:40])
    else:
        print('not found:', old[:40])
open(p, 'w', encoding='utf-8').write(s)
