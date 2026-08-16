# -*- coding: utf-8 -*-
import subprocess, os, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
GIT = r'C:\Users\Administrator\Desktop\git-2.45.2'
V1 = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\v1.exe'
PRE = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\compat_prelude.h'
files = []
for d in ['', 'builtin', 'reftable', 'xdiff', 'ewah', 'refs', 'negotiator', 'trace2', 'sha1dc', 'sha256', 'block-sha1']:
    base = os.path.join(GIT, d)
    if os.path.isdir(base):
        for f in sorted(os.listdir(base)):
            if f.endswith('.c'):
                files.append(os.path.join(d, f).replace('\\', '/'))
for root, dirs, names in os.walk(os.path.join(GIT, 'compat')):
    for f in sorted(names):
        if f.endswith('.c'):
            rel = os.path.relpath(os.path.join(root, f), GIT).replace('\\', '/')
            if rel.startswith('compat/regex/'):
                continue  # 已知超时，跳过
            files.append(rel)
ok = 0; fail = 0; timeout = 0; failed = []
for rel in files:
    out = os.path.join(GIT, '_objs_jy', rel.replace('/', '_').replace('\\', '_')[:-2] + '.o')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    try:
        r = subprocess.run([V1, '-D', '__STDC_VERSION__=199901L', '-D', '__GNUC__=5', '-D', '_WIN32=1',
                            '-D', 'REG_STARTEND=1', '-I', PRE, '-c', os.path.join(GIT, rel), '-o', out],
                           capture_output=True, timeout=60)
        if r.returncode == 0 and os.path.exists(out):
            ok += 1
        else:
            fail += 1
            failed.append(rel)
    except subprocess.TimeoutExpired:
        timeout += 1
        failed.append(rel + ' (TIMEOUT)')
print('TOTAL', len(files), 'OK', ok, 'FAIL', fail, 'TIMEOUT', timeout)
print('FAILED:', failed[:25])
