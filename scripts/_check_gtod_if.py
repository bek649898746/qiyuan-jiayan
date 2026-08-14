# -*- coding: utf-8 -*-
import io, sys, re
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
lines = open(r'C:\Users\Administrator\Desktop\git-2.45.2\compat\mingw.c', encoding='utf-8', errors='replace').read().split('\n')
# gettimeofday 在 1089 行，看前面 100 行的条件编译
for i, l in enumerate(lines, 1):
    if i >= 989 and i <= 1089:
        if re.match(r'\s*#\s*(if|ifdef|ifndef|else|endif)', l):
            print(f'{i}: {l.strip()[:80]}')
    if 'gettimeofday' in l:
        print(f'{i}: {l.strip()[:80]}  ← gettimeofday')
