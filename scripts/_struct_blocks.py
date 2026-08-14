# -*- coding: utf-8 -*-
import io, sys, re
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
src = open('merge-ort.c', encoding='utf-8', errors='replace').read()
lines = src.split('\n')
# 找 struct 密集区域：统计每行 struct 出现，找连续 struct 行块
struct_lines = [i for i, l in enumerate(lines, 1) if re.search(r'\bstruct\b', l)]
# 找连续行块（间隔 <= 1 行）
blocks = []
start = struct_lines[0]; prev = struct_lines[0]
for ln in struct_lines[1:]:
    if ln - prev <= 2:
        prev = ln
    else:
        blocks.append((start, prev)); start = ln; prev = ln
blocks.append((start, prev))
print('struct 密集块 (行范围: 行数):')
for s, e in blocks:
    if e - s >= 5:
        print(f'  行 {s}-{e} ({e-s+1} 行)')
        for i in range(s-1, min(e, s+4)):
            print(f'    {i+1}: {lines[i][:90]}')
        print('    ...')
        for i in range(max(s, e-2), e):
            print(f'    {i+1}: {lines[i][:90]}')
