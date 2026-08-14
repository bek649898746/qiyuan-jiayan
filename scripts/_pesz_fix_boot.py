# -*- coding: utf-8 -*-
"""确认当前 str_tbl 状态 + 指针p_esz修复后自举"""
import io, sys, os, subprocess
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

# 确认 str_tbl 状态
h = open('srclib/qcc_x86.c','rb').read()
print('宿主 str_tbl:', b'str_tbl[2048][2048]' in h, '(2048)' if b'str_tbl[2048][2048]' in h else '(1024)')
m = open('srclib_jiayan/qcc_work.jy','rb').read()
print('镜像 str_tbl:', b'str_tbl[2048][2048]' in m, '(2048)' if b'str_tbl[2048][2048]' in m else '(1024)')

def run(cmd, timeout=300):
    r = subprocess.run(cmd, capture_output=True, timeout=timeout)
    return r.returncode, (r.stdout + r.stderr).decode('utf-8', errors='replace')

rc, out = run(['gcc', '-O2', '-Wall', '-Werror', 'srclib/qcc_x86.c', '-o', 'qcc_x86.exe'])
print('gcc rc=%d' % rc)
if rc != 0: print(out[-200:]); sys.exit(1)
for src, dst in [('qcc_x86.exe','v1.exe'), ('v1.exe','v2.exe'), ('v2.exe','v3.exe'), ('v3.exe','v4.exe')]:
    rc, out = run([r'.\%s' % src, 'srclib_jiayan/qcc_work.jy', '-o', dst])
    print('%s->%s rc=%d' % (src, dst, rc))
    if rc != 0: print(out[-200:]); sys.exit(1)
import hashlib
hs = {}
for f in ['v1.exe','v2.exe','v3.exe','v4.exe']:
    hs[f] = hashlib.sha256(open(f,'rb').read()).hexdigest()
    print('%s %s' % (f, hs[f][:12]))
vals = set(hs.values())
print('FIXPOINT:', 'OK' if len(vals)==1 else 'DIVERGE (%d)' % len(vals))
