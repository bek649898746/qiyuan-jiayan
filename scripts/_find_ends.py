# -*- coding: utf-8 -*-
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
lines = open('merge-ort.c', encoding='utf-8', errors='replace').read().split('\n')
for i, l in enumerate(lines, 1):
    if 'ends_with' in l or 'skip_to_optional_arg' in l or 'starts_with_mem' in l:
        print(f'{i}: {l.strip()[:100]}')
