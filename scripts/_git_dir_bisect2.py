# -*- coding: utf-8 -*-
"""dir.c 细二分."""
import subprocess, os, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
GIT = r'C:\Users\Administrator\Desktop\git-2.45.2'
QCC = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\qcc_x86.exe'
D = ['-D', '__STDC_VERSION__=199901L', '-D', '__GNUC__=5', '-D', '_WIN32=1', '-D', 'REG_STARTEND=1']
lines = open(os.path.join(GIT, 'dir.c'), 'rb').read().decode('utf-8', errors='replace').splitlines()

def tc(n):
    src = '\n'.join(lines[:n]) + '\n'
    p = os.path.join(GIT, '_dp.c')
    open(p, 'w', encoding='utf-8').write(src)
    r = subprocess.run([QCC] + D + ['_dp.c'], capture_output=True, timeout=180, cwd=GIT)
    print('n=%d rc=%d %s' % (n, r.returncode, 'OK' if r.returncode == 0 else 'FAIL(0x%X)' % (r.returncode & 0xFFFFFFFF)))
    return r.returncode == 0

# 二分 500-1000
lo, hi = 500, 1000
if tc(500):
    lo = 500
else:
    hi = 500
if hi > lo and not tc(lo):
    pass
# 更细: 在 [lo, hi] 内找边界
while hi - lo > 50:
    mid = (lo + hi) // 2
    if tc(mid):
        lo = mid
    else:
        hi = mid
print('崩溃边界: 大约 行 %d-%d' % (lo, hi))
for i in range(lo, min(hi + 3, len(lines))):
    print('  %d: %s' % (i + 1, lines[i][:100]))
