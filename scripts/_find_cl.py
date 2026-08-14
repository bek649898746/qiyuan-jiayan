# -*- coding: utf-8 -*-
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
lines = open('merge-ort.c', encoding='utf-8', errors='replace').read().split('\n')
for i, l in enumerate(lines, 1):
    if 'commit_list' in l or 'object_id' in l and 'struct' in l and '(' in l:
        print(f'{i}: {l.strip()[:110]}')
