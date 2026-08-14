# -*- coding: utf-8 -*-
"""dir.c 二分: 截取前 N 行编译, 定位崩溃区域."""
import subprocess, os, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
GIT = r'C:\Users\Administrator\Desktop\git-2.45.2'
QCC = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\qcc_x86.exe'
D = ['-D', '__STDC_VERSION__=199901L', '-D', '__GNUC__=5', '-D', '_WIN32=1', '-D', 'REG_STARTEND=1']

lines = open(os.path.join(GIT, 'dir.c'), 'rb').read().decode('utf-8', errors='replace').splitlines()
print('total', len(lines))

def try_compile(n, tag):
    body = lines[:n]
    src = '\n'.join(body) + '\n'
    p = os.path.join(GIT, '_dirpart.c')
    open(p, 'w', encoding='utf-8').write(src)
    r = subprocess.run([QCC] + D + [os.path.basename(p)], capture_output=True, timeout=120, cwd=GIT)
    err = (r.stdout + r.stderr).decode('utf-8', 'replace')
    ok = r.returncode == 0
    print('%s: n=%d rc=%d %s' % (tag, n, r.returncode, 'OK' if ok else 'FAIL'))
    if not ok:
        for line in err.splitlines():
            if '[ERR]' in line:
                print('   ', line.strip()[:90])
                break
    return ok

try_compile(1000, 'Q1')
try_compile(2000, 'Q2')
try_compile(3000, 'Q3')
