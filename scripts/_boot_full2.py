# -*- coding: utf-8 -*-
"""完整自举: 宿主无编码器 + 镜像有编码器"""
import subprocess, os, hashlib, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

def run(cmd, timeout=300):
    r = subprocess.run(cmd, capture_output=True, timeout=timeout)
    return r.returncode, (r.stdout + r.stderr).decode('utf-8', errors='replace')

for src, dst in [('v1.exe','v2.exe'), ('v2.exe','v3.exe'), ('v3.exe','v4.exe')]:
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
