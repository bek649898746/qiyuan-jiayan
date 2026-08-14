# -*- coding: utf-8 -*-
"""v1 编 v2 (删regn后)"""
import subprocess, os, hashlib, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
r = subprocess.run([r'.\v1.exe', 'srclib_jiayan/qcc_work.jy', '-o', 'v2.exe'], capture_output=True, timeout=300)
print('v1->v2 rc=%d' % r.returncode)
if r.returncode == 0:
    print('v2 hash:', hashlib.sha256(open('v2.exe','rb').read()).hexdigest()[:12])
else:
    print((r.stdout + r.stderr).decode('utf-8', errors='replace')[-200:])
