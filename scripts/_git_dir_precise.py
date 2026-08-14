# -*- coding: utf-8 -*-
"""精确测 60-125 每 10 行."""
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
    r = subprocess.run([QCC] + D + ['_dp.c'], capture_output=True, timeout=120, cwd=GIT)
    ok = r.returncode == 0
    print('n=%d rc=%d %s' % (n, r.returncode, 'OK' if ok else 'FAIL(0x%X)' % (r.returncode & 0xFFFFFFFF)))
    return ok

last_ok = 0
for n in [80, 90, 100, 105, 110, 115, 120, 122]:
    if tc(n):
        last_ok = n
    else:
        break
print('边界: %d OK, 下一个 FAIL' % last_ok)
for i in range(last_ok, min(last_ok + 8, len(lines))):
    print('  %d: %s' % (i + 1, lines[i][:110]))
