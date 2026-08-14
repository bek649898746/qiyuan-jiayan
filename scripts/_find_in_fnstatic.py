# -*- coding: utf-8 -*-
import re
m = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', encoding='utf-8').read()
lines = m.split(chr(10))
# Track function boundaries by brace depth from '静 整 NAME(...){'
# Find all function definitions
fn_starts = []
for i, l in enumerate(lines):
    if re.search(r'静\s+\w+\s+\w+\s*\([^)]*\)\s*\{', l) and 'strcpy' not in l and 'static' not in l:
        fn_starts.append(i)
print('functions:', len(fn_starts))
# For each function, find its body range and local static decls
# simple: scan forward tracking brace depth
fn_static = []
for fi, start in enumerate(fn_starts):
    depth = 0
    body = False
    for i in range(start, min(start + 2000, len(lines))):
        l = lines[i]
        if '{' in l:
            depth += l.count('{')
            body = True
        if '}' in l:
            depth -= l.count('}')
        if body and depth <= 0:
            end = i
            break
    else:
        end = min(start + 2000, len(lines))
    for i in range(start, end):
        l = lines[i]
        mm = re.search(r'(^|[;{]\s*)静\s+(?:整|字|无\s+字|双)\s+(\w+)\s*([;,=\[])', l)
        if mm and ' 空 ' not in l:
            fn_static.append((mm.group(2), start+1, i+1))
print('function-local static decls:', len(fn_static))
# find duplicates
from collections import Counter
c = Counter(nm for nm, _, _ in fn_static)
dups = {k: v for k, v in c.items() if v > 1}
print('duplicates:', len(dups))
for k, v in dups.items():
    print(' ', k, v)
# also list all names
print('--- all function-local static names ---')
for nm, fs, ln in fn_static:
    print(' ', nm, 'fn@%d decl@%d' % (fs, ln))
