# -*- coding: utf-8 -*-
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
lines = open('merge-ort.c', encoding='utf-8', errors='replace').read().split('\n')
for i, l in enumerate(lines, 1):
    if 'merge_options_internal' in l or ('repo;' in l and 'struct' in l) or l.strip().startswith('struct commit;') or l.strip().startswith('struct object_id;'):
        print(f'{i}: {l.strip()[:100]}')
