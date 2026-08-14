# -*- coding: utf-8 -*-
import re
m = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', encoding='utf-8').read()
lines = m.split(chr(10))
# find function-level static declarations: 静 整 x; / 静 字 x[..]; inside functions
# rough: lines with 静 整 / 静 字 / 静 双 / 静 构 that declare a variable (not function)
# collect variable names
var_names = {}
for i, l in enumerate(lines):
    # function-local static: 静 整 NAME / 静 字 NAME  (not 静 空 NAME( = function)
    if re.search(r'静\s+(整|字|无\s+字|双)\s+(\w+)\s*([;,=[])', l):
        mm = re.search(r'静\s+(?:整|字|无\s+字|双)\s+(\w+)', l)
        if mm:
            nm = mm.group(1)
            if nm not in ('空',):
                var_names.setdefault(nm, []).append(i+1)
dups = {k: v for k, v in var_names.items() if len(v) > 1}
print('static var names with >1 declaration:', len(dups))
for k, v in list(dups.items())[:30]:
    print(' ', k, v)
