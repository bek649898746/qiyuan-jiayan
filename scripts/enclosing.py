# -*- coding: utf-8 -*-
"""找某行所在的函数定义（向上扫描第一个「静 ...(...) {」形态）。"""
import io
import re
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
line_no = int(sys.argv[1])
lines = io.open('srclib_jiayan/qcc_zh.jy', encoding='utf-8', errors='replace').read().split('\n')
rx = re.compile(r'^(静|整|空|长 长|字) .*\(.*\) *\{? *$')
for n in range(line_no - 1, 0, -1):
    t = lines[n - 1]
    if rx.match(t) and '(' in t and ('{' in t or ';' in t):
        print('%d: %s' % (n, t[:200]))
        break
