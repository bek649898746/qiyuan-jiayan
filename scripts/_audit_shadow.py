# -*- coding: utf-8 -*-
"""遮蔽审计: 镜像函数作用域下, 局部/参数变量名 == 全局数组名 → 遮蔽风险 (知识库 11.5).

方法: 提取镜像全局数组名集合; 按函数拆分, 收集每函数内局部声明+参数名;
若函数内出现 `name[` 数组访问且该函数有同名局部/参数 → 报告 (该访问会匹配局部而非全局).
"""
import re, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

SRC = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy'
lines = open(SRC, encoding='utf-8', errors='replace').read().split('\n')

# 1. 全局数组名集合: 顶层 '静 整 name[...]' / '静 字 name[...]' / '静 常 字 name[...]'
global_arr = set()
for l in lines:
    m = re.search(r'^静 .*?([A-Za-z_]\w*)\[', l)
    if m and ('整' in l or '字' in l or '常' in l):
        global_arr.add(m.group(1))
# per-node 动态指针 (n0-n255 等)
for i in range(0, 256):
    global_arr.add(f'n{i}')
global_arr |= {'pesz', 'nt', 'nll', 'ndbl', 'nuns', 'nn', 'nv', 'n10',
               'tt', 'tv', 'tn', 'tuns', 'tll', 'tll_hi', 'str_tbl', 'vars'}

# 2. 按函数拆分 + 局部/参数收集
fn_pat = re.compile(r'^静 (整|空|字|无|常 整|常 字) ([A-Za-z_]\w*)\(')
fn = None
fn_locals = {}   # fn -> set of local names
fn_arrays = {}   # fn -> set of array names accessed as name[
cur = None
depth = 0
func_order = []

for i, l in enumerate(lines, 1):
    m = fn_pat.match(l)
    if m:
        cur = m.group(2)
        func_order.append(cur)
        fn_locals[cur] = set()
        fn_arrays[cur] = set()
        # 参数名
        params = l.split('(')[1]
        for pm in re.findall(r'(?:常 )?(?:整|字|空|无|指|结构\w*)\s+([A-Za-z_]\w*)', params.split(')')[0]):
            fn_locals[cur].add(pm)
        continue
    if cur is None:
        continue
    # 函数体内: 局部声明
    for dm in re.finditer(r'(?:^|\s)(?:整|字|无|常 整|静 整|静 字|结构\w+)\s+([A-Za-z_]\w*)(?:\s*[=;,])', l):
        nm = dm.group(1)
        if nm not in global_arr or nm in ('main', 'prim', 'stmt', 'expr'):
            fn_locals[cur].add(nm)
    # 数组访问 name[
    for am in re.finditer(r'\b([A-Za-z_]\w*)\[', l):
        if am.group(1) in global_arr:
            fn_arrays[cur].add(am.group(1))

# 3. 报告冲突
print('=== 遮蔽审计: 全局数组集合 (%d) ===' % len(global_arr))
print(sorted(global_arr))
print()
conflicts = 0
for fn in func_order:
    loc = fn_locals.get(fn, set())
    acc = fn_arrays.get(fn, set())
    for g in acc:
        if g in loc:
            print(f'[遮蔽] 函数 {fn}: 访问全局数组 {g}[], 但函数内有同名局部/参数!')
            conflicts += 1
print(f'\n共 {conflicts} 处遮蔽风险')
