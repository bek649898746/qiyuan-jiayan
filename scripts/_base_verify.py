# -*- coding: utf-8 -*-
"""确认 3013a44c 基线 (v1==v2)"""
import subprocess, os, hashlib, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

def run(cmd, timeout=300):
    r = subprocess.run(cmd, capture_output=True, timeout=timeout)
    return r.returncode, (r.stdout + r.stderr).decode('utf-8', errors='replace')

rc, out = run(['gcc', '-O2', '-Wall', '-Werror', 'srclib/qcc_x86.c', '-o', 'qcc_x86.exe'])
print('gcc rc=%d' % rc)
if rc != 0: print(out[-200:]); sys.exit(1)
for src, dst in [('qcc_x86.exe','v1.exe'), ('v1.exe','v2.exe')]:
    rc, out = run([r'.\%s' % src, 'srclib_jiayan/qcc_work.jy', '-o', dst])
    print('%s->%s rc=%d' % (src, dst, rc))
    if rc != 0: print(out[-200:]); sys.exit(1)
h1 = hashlib.sha256(open('v1.exe','rb').read()).hexdigest()
h2 = hashlib.sha256(open('v2.exe','rb').read()).hexdigest()
print('v1=%s v2=%s %s' % (h1[:12], h2[:12], 'SAME' if h1 == h2 else 'DIFF'))
