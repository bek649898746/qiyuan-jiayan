# -*- coding: utf-8 -*-
"""str_tbl 2048 全一致 (数组+守卫) 自举"""
import subprocess, os, hashlib, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

def run(cmd, timeout=300):
    r = subprocess.run(cmd, capture_output=True, timeout=timeout)
    return r.returncode, (r.stdout + r.stderr).decode('utf-8', errors='replace')

rc, out = run(['gcc', '-O2', '-Wall', '-Werror', 'srclib/qcc_x86.c', '-o', 'qcc_x86.exe'])
print('gcc rc=%d' % rc)
if rc != 0:
    print(out[-300:]); sys.exit(1)
for src, dst in [('qcc_x86.exe','v1.exe'), ('v1.exe','v2.exe'), ('v2.exe','v3.exe'), ('v3.exe','v4.exe')]:
    rc, out = run([r'.\%s' % src, 'srclib_jiayan/qcc_work.jy', '-o', dst])
    print('%s->%s rc=%d' % (src, dst, rc))
    if rc != 0:
        print(out[-200:]); sys.exit(1)
hs = {}
for f in ['v1.exe','v2.exe','v3.exe','v4.exe']:
    hs[f] = hashlib.sha256(open(f,'rb').read()).hexdigest()
    print('%s %s' % (f, hs[f][:12]))
vals = set(hs.values())
print('FIXPOINT:', 'OK' if len(vals)==1 else 'DIVERGE (%d)' % len(vals))
