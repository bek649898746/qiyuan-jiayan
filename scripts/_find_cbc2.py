# -*- coding: utf-8 -*-
"""精确搜 HEAD 的 CODE_BUF_CAP 定义"""
import io, sys, subprocess
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
r = subprocess.run(['git','show','HEAD:srclib/qcc_x86.c'], capture_output=True)
head = r.stdout.decode('utf-8', errors='replace')
lines = head.split('\n')
for i, l in enumerate(lines, 1):
    if 'CODE_BUF_CAP' in l:
        print('%5d: %s' % (i, l.strip()[:110]))
