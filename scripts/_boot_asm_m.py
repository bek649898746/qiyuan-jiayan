# -*- coding: utf-8 -*-
"""宿主编镜像 (验证镜像编码器语法)"""
import subprocess, os, io, sys, hashlib
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')

def run(cmd, timeout=300):
    r = subprocess.run(cmd, capture_output=True, timeout=timeout)
    return r.returncode, (r.stdout + r.stderr).decode('utf-8', errors='replace')

rc, out = run(['gcc', '-O2', '-Wall', '-Werror', 'srclib/qcc_x86.c', '-o', 'qcc_x86.exe'])
print('gcc rc=%d' % rc)
if rc != 0:
    print(out[-300:]); sys.exit(1)
rc, out = run([r'.\qcc_x86.exe', 'srclib_jiayan/qcc_work.jy', '-o', 'v1.exe'])
print('宿主->v1 rc=%d' % rc)
if rc != 0:
    print(out[-400:])
else:
    print('v1 hash:', hashlib.sha256(open('v1.exe','rb').read()).hexdigest()[:12])
