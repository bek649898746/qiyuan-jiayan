# -*- coding: utf-8 -*-
"""宿主 str_tbl 2048 (字节模式), 编 v1, gdb 抓崩溃"""
import io, sys, os
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

p = 'srclib/qcc_x86.c'
d = open(p, 'rb').read()
d = d.replace(b'static char str_tbl[1024][2048]', b'static char str_tbl[2048][2048]')
open(p, 'wb').write(d)
print('宿主 str_tbl -> 2048')

# 编译宿主 + v1 (应崩)
import subprocess
r = subprocess.run(['gcc', '-O2', '-Wall', '-Werror', 'srclib/qcc_x86.c', '-o', 'qcc_x86.exe'], capture_output=True, text=True, encoding='utf-8', errors='replace')
print('gcc rc=%d' % r.returncode)
if r.returncode != 0:
    print(r.stderr[-200:])
    sys.exit(1)
r2 = subprocess.run([r'.\qcc_x86.exe', 'srclib_jiayan/qcc_work.jy', '-o', 'v1.exe'], capture_output=True, timeout=300)
print('宿主->v1 rc=%d' % r2.returncode)
