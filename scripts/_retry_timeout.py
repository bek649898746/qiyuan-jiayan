# -*- coding: utf-8 -*-
"""用长超时(300s)编译 TIMEOUT 文件, 区分慢 vs 真挂. flush 输出."""
import subprocess, os, io, sys, functools
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
print = functools.partial(print, flush=True)
GIT = r'C:\Users\Administrator\Desktop\git-2.45.2'
QCC = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\qcc_x86.exe'
PRE = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\compat_prelude.h'
D = ['-D', '__STDC_VERSION__=199901L', '-D', '__GNUC__=5', '-D', '_WIN32=1', '-D', 'REG_STARTEND=1']

files = [
'rerere.c','run-command.c','xdiff/xpatience.c',
'compat/precompose_utf8.c','compat/fsmonitor/fsm-health-win32.c','compat/fsmonitor/fsm-ipc-darwin.c',
]
objdir = os.path.join(GIT, '_objs')
for rel in files:
    p = os.path.join(GIT, rel.replace('/', os.sep))
    out = os.path.join(objdir, rel.replace('/', '_').replace('\\', '_')[:-2] + '.o')
    if os.path.exists(out):
        print('SKIP ' + rel + ' (already have .o)')
        continue
    args = [QCC] + D + ['-I', PRE, '-c', p, '-o', out]
    try:
        r = subprocess.run(args, capture_output=True, timeout=600, cwd=GIT)
        err = (r.stdout + r.stderr).decode('utf-8', 'replace')
        if r.returncode == 0 and os.path.exists(out):
            print('OK   ' + rel)
        else:
            print('FAIL ' + rel + ' rc=' + str(r.returncode))
            tail = [l for l in err.splitlines() if l.strip()][-3:]
            for t in tail:
                print('       ' + t[:160])
    except subprocess.TimeoutExpired:
        print('STILL-TIMEOUT ' + rel + ' (>300s)')
print('==== done ====')
