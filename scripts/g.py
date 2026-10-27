# -*- coding: utf-8 -*-
"""g.py —— 源码检索小工具（UTF-8 安全，PowerShell 里可直接带中文）

用法:
  python -X utf8 scripts/g.py <正则> [文件...] [-C n]

为什么不用 Select-String: PowerShell 控制台编码会把中文参数弄坏。
"""
import io
import os
import re
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

args = sys.argv[1:]
ctx = 0
if '-C' in args:
    i = args.index('-C')
    ctx = int(args[i + 1])
    del args[i:i + 2]
pat = args[0]
files = args[1:] or [os.path.join('srclib_jiayan', 'qcc_zh.jy')]
rx = re.compile(pat)
expanded = []
for f in files:
    if os.path.isdir(f):
        for base, _dirs, names in os.walk(f):
            for nm in names:
                if nm.endswith(('.c', '.jy', '.py', '.md', '.ps1', '.h')):
                    expanded.append(os.path.join(base, nm))
    else:
        expanded.append(f)
files = expanded
for f in files:
    lines = io.open(f, encoding='utf-8', errors='replace').read().split('\n')
    for n, ln in enumerate(lines):
        if rx.search(ln):
            lo = max(0, n - ctx)
            hi = min(len(lines), n + ctx + 1)
            for k in range(lo, hi):
                mark = '>' if k == n else ' '
                print('%s%d:%s' % (mark, k + 1, lines[k]))
            if ctx:
                print('--')
