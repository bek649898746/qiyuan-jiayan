# -*- coding: utf-8 -*-
import io, sys, re
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
lines = open('merge-ort.c', encoding='utf-8', errors='replace').read().split('\n')
# 找连续前向声明块 struct X;
for i in range(len(lines)):
    l = lines[i].strip()
    if re.match(r'^struct\s+\w+\s*;$', l):
        # 打印前后各 3 行
        print(f'=== 前向声明块 @ {i+1} ===')
        for j in range(max(0, i-2), min(len(lines), i+8)):
            print(f'{j+1}: {lines[j].strip()[:90]}')
        break
