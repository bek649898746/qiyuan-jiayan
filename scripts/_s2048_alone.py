# -*- coding: utf-8 -*-
"""str_tbl 2048 单独: 编宿主 -> v1 -> v2, 抓崩溃"""
import subprocess, os, io, sys, hashlib
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

def run(cmd, timeout=300):
    r = subprocess.run(cmd, capture_output=True, timeout=timeout)
    return r.returncode, (r.stdout + r.stderr).decode('utf-8', errors='replace')

rc, out = run(['gcc', '-O2', '-Wall', '-Werror', 'srclib/qcc_x86.c', '-o', 'qcc_x86.exe'])
print('gcc rc=%d' % rc)
if rc != 0: print(out[-300:]); sys.exit(1)
rc, out = run([r'.\qcc_x86.exe', 'srclib_jiayan/qcc_work.jy', '-o', 'v1.exe'])
print('宿主->v1 rc=%d' % rc)
if rc != 0: print(out[-200:]); sys.exit(1)
rc, out = run([r'.\v1.exe', 'srclib_jiayan/qcc_work.jy', '-o', 'v2.exe'])
print('v1->v2 rc=%d' % rc)
if rc != 0:
    print(out[-200:])
