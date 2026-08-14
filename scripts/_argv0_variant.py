# -*- coding: utf-8 -*-
"""测试不同 argv[0] 形式对输出变体的影响 + 运行验证."""
import subprocess, hashlib, os, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.chdir(r'C:\Users\Administrator\Desktop\qiyuan-jiayan')
exp = '10\n3000000000\n3.14\n15 1705032704'

def run_exe(path):
    p = subprocess.run(['cmd', '/c', path], capture_output=True, text=True, timeout=15)
    return (p.stdout or '').strip() == exp

variants = {
    'no-path    ': ['qcc_x86.exe'],
    'dot-rel    ': ['.\\qcc_x86.exe'],
    'abs        ': [os.path.abspath('qcc_x86.exe')],
    'dotdot-rel ': ['..\\qcc_x86.exe'],  # 从 scratch_test 跑
}
for tag, cmd in variants.items():
    out = 'scratch_test/vt.exe'
    if tag == 'dotdot-rel ':
        cwd = 'scratch_test'
    else:
        cwd = None
    r = subprocess.run(cmd + ['tests/behavior/b_global.c', '-o', out], cwd=cwd, capture_output=True)
    if r.returncode != 0:
        print(tag, 'compile rc=', r.returncode); continue
    d = open(out, 'rb').read()
    ok = run_exe(out)
    print(f'{tag} sha={hashlib.sha256(d).hexdigest()[:16]} size={len(d)} run_ok={ok}')
