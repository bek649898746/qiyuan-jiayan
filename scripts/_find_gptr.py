# -*- coding: utf-8 -*-
"""搜镜像的全局指针变量 (++ 可能 32 位的)"""
import io, sys, re
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
t = open(r'C:\Users\Administrator\Desktop\qiyuan-jiayan\srclib_jiayan\qcc_work.jy', 'rb').read().decode('utf-8-sig', errors='replace')
lines = t.split('\n')
# 全局 字 *xxx 或 整 *xxx (顶层声明)
for i, l in enumerate(lines, 1):
    if i < 1200 and re.search(r'^(静 )?字 \*|^(静 )?整 \*|^(静 )?构.*\*', l.strip()):
        print('%5d: %s' % (i, l.strip()[:100]))
