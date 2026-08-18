# -*- coding: utf-8 -*-
"""自举五代 + SHA 校验"""
import subprocess, hashlib, os, sys

os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
for i in range(1, 6):
    src = 'qcc_x86.exe' if i == 1 else f'v{i-1}.exe'
    r = subprocess.run([os.path.abspath(src), 'srclib_jiayan/qcc_work.jy', '-o', f'v{i}.exe'],
                       capture_output=True, text=True, errors='replace')
    if r.returncode != 0:
        print(f'v{i} FAIL: {r.stdout[-200:]} {r.stderr[-200:]}')
        sys.exit(1)
    print(f'v{i}: {r.stdout.strip()[-40:]}')
hs = [hashlib.sha256(open(f'v{i}.exe', 'rb').read()).hexdigest() for i in range(1, 6)]
for i, h in enumerate(hs, 1):
    print(f'v{i}: {h[:16]}...')
print('ALL EQUAL:', len(set(hs)) == 1)
print('FIXED POINT:', hs[0][:8])
