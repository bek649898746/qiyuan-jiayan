# -*- coding: utf-8 -*-
import re
m = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', encoding='utf-8').read()
lines = m.split(chr(10))
# find function definitions to determine function boundaries
# find all 静 整 NAME(  function defs and local static declarations
# Track: when we see '静 整 NAME(空) {' or similar, we're in a function
fns = []
for i, l in enumerate(lines):
    if re.search(r'静\s+\w+\s+\w+\s*\([^)]*\)\s*\{', l) and 'strcpy' not in l:
        fns.append((i+1, l.strip()[:60]))
print('function defs:', len(fns))
# find local static declarations: 静 整 NAME; or 静 字 NAME[..]; inside functions
# Heuristic: 静 followed by type then name then ; or [
locals_static = {}
for i, l in enumerate(lines):
    if re.search(r'(^|[;{]\s*)静\s+(整|字|无\s+字|双)\s+(\w+)\s*([;,=\[])', l):
        mm = re.search(r'静\s+(?:整|字|无\s+字|双)\s+(\w+)', l)
        if mm and mm.group(1) not in ('空',):
            locals_static.setdefault(mm.group(1), []).append(i+1)
print('local static declarations:', len(locals_static))
dups = {k: v for k, v in locals_static.items() if len(v) > 1}
print('duplicate local static names:', len(dups))
for k, v in list(dups.items())[:20]:
    print(' ', k, v)
