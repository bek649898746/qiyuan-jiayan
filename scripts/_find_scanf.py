# -*- coding: utf-8 -*-
"""找镜像 scanf 相关位置."""
import io, sys, re
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
lines = open('srclib_jiayan/qcc_work.jy', encoding='utf-8', errors='replace').read().split('\n')
for i, l in enumerate(lines):
    if 'emit_print' in l:
        print('CG:', i + 1, repr(l[:130]))
    if '"putstr"' in l:
        print('BUILTIN:', i + 1, repr(l[:150]))
