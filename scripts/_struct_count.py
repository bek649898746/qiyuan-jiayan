# -*- coding: utf-8 -*-
import io, sys, re
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
for f in ['merge-ort.c', 'sequencer.c']:
    src = open(f, encoding='utf-8', errors='replace').read()
    # struct 定义: struct NAME {  (含 typedef struct NAME {)
    defs = re.findall(r'\bstruct\s+([A-Za-z_]\w*)\s*\{', src)
    # 匿名 struct: struct {
    anon = len(re.findall(r'\bstruct\s*\{', src))
    # typedef struct {...} NAME
    # 去重 struct 定义名
    print('===', f, '===')
    print('  struct 定义 (struct X {):', len(defs), '去重:', len(set(defs)))
    print('  匿名 struct {:', anon)
    print('  定义名列表:', sorted(set(defs))[:80])
    # 最大函数（按 { } 深度或行数）
    lines = src.split('\n')
    # 找函数定义: 返回类型 名称 ( ... ) { 在行首
    print('  总行数:', len(lines))
