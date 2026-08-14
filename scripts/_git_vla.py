# -*- coding: utf-8 -*-
"""精确检测 VLA: 声明中数组大小是非字面量表达式."""
import os, sys, re, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
ROOT = r'C:\Users\Administrator\Desktop\git-2.45.2'

# 声明模式: [类型] 名字 [非字面量] ;  排除 下标访问 (不在声明位置)
# 声明: \b(?:int|char|unsigned|long|short|struct\s+\w+|unsigned\s+\w+|const\s+\w+)\s+(\w+)\s*\[\s*(\w+|[^0-9][^\[\]]*)\s*\]
decl_pat = re.compile(r'\b(?:int|char|unsigned|long|short|size_t|struct\s+\w+|const\s+\w+)\s+(\w+)\s*\[\s*([a-zA-Z_][a-zA-Z0-9_]*(?:\s*[+\-]\s*[a-zA-Z0-9_]+)?|[a-zA-Z_]\w*\s*\+\s*\d+)\s*\]\s*;')
# 排除宏/typedef 定义误报, 仅看 .c 声明语句

total = 0
by_file = {}
for dirpath, dirs, fs in os.walk(ROOT):
    if '.git' in dirpath: continue
    for f in fs:
        if not f.endswith('.c'): continue
        p = os.path.join(dirpath, f)
        t = open(p, 'rb').read().decode('utf-8', errors='replace')
        ms = decl_pat.findall(t)
        if ms:
            by_file[f] = ms
            total += len(ms)

print('精确 VLA 声明总数:', total, '| 涉及文件:', len(by_file))
for f, ms in sorted(by_file.items())[:15]:
    print(' ', f, len(ms))
    for m in ms[:4]:
        print('    ', m)
