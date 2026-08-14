# -*- coding: utf-8 -*-
"""逐步 include dir.c 的头, 定位栈溢出源头."""
import subprocess, os, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
GIT = r'C:\Users\Administrator\Desktop\git-2.45.2'
QCC = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\qcc_x86.exe'
D = ['-D', '__STDC_VERSION__=199901L', '-D', '__GNUC__=5', '-D', '_WIN32=1', '-D', 'REG_STARTEND=1']

hds = [l.strip().split('"')[1] for l in open(os.path.join(GIT, 'dir.c'), 'rb').read().decode('utf-8', errors='replace').splitlines()[:32] if l.startswith('#include "')]
print('headers:', len(hds))

p = os.path.join(GIT, '_hdrtest.c')
for i in range(1, len(hds) + 1):
    src = '\n'.join('#include "%s"' % h for h in hds[:i]) + '\nint _x;\n'
    open(p, 'w', encoding='utf-8').write(src)
    r = subprocess.run([QCC] + D + ['_hdrtest.c'], capture_output=True, timeout=60, cwd=GIT)
    if r.returncode != 0:
        print('FAIL at include #%d: %s (rc=%d)' % (i, hds[i-1], r.returncode))
        break
    print('OK  #%d: %s' % (i, hds[i-1]))
else:
    print('ALL HEADERS OK')
