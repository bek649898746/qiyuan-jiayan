# -*- coding: utf-8 -*-
"""镜像局部 pesz 改名 peszl — 消除对全局数组 pesz 的遮蔽 (函数作用域)."""
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
p = 'srclib_jiayan/qcc_work.jy'
s = open(p, encoding='utf-8').read()

repl = [
    ('整 pesz=var_pesz(vn);', '整 peszl=var_pesz(vn);'),
    ('若(!did && pesz>0 && var_arrsz(vn)==0)', '若(!did && peszl>0 && var_arrsz(vn)==0)'),
    ('整 pesz = var_pesz(vname);', '整 peszl = var_pesz(vname);'),
    ('若 (!did && pesz > 0 && var_arrsz(vname) == 0)', '若 (!did && peszl > 0 && var_arrsz(vname) == 0)'),
]
for old, new in repl:
    if old not in s:
        print('NOT FOUND:', old[:50])
    else:
        s = s.replace(old, new)
        print('OK:', old[:50])

open(p, 'w', encoding='utf-8').write(s)
print('done')
