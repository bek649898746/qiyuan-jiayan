# -*- coding: utf-8 -*-
"""重试编译缺 .o 的源文件, 分类 FAIL/CRASH/TIMEOUT 并打印错误摘要."""
import subprocess, os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
import functools
print = functools.partial(print, flush=True)
GIT = r'C:\Users\Administrator\Desktop\git-2.45.2'
QCC = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\qcc_x86.exe'
PRE = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\compat_prelude.h'
D = ['-D', '__STDC_VERSION__=199901L', '-D', '__GNUC__=5', '-D', '_WIN32=1', '-D', 'REG_STARTEND=1']

missing = [
'rerere.c','run-command.c','builtin/help.c','xdiff/xpatience.c',
'compat/precompose_utf8.c','compat/fsmonitor/fsm-health-win32.c','compat/fsmonitor/fsm-ipc-darwin.c',
]
objdir = os.path.join(GIT, '_objs')
results = {'ok': [], 'fail': [], 'crash': [], 'timeout': []}
for rel in missing:
    p = os.path.join(GIT, rel.replace('/', os.sep))
    out = os.path.join(objdir, rel.replace('/', '_').replace('\\', '_')[:-2] + '.o')
    args = [QCC] + D + ['-I', PRE]
    if rel == 'compat/regex/regex.c':
        args += ['-I', os.path.join(GIT, 'compat', 'regex', 'regex.h'), '-D', 'NO_MBSUPPORT']
    args += ['-c', p, '-o', out]
    try:
        r = subprocess.run(args, capture_output=True, timeout=30, cwd=GIT)
        err = (r.stdout + r.stderr).decode('utf-8', 'replace')
        if r.returncode == 0 and os.path.exists(out):
            results['ok'].append(rel)
            print('OK   ' + rel)
        else:
            results['fail'].append(rel)
            print('FAIL ' + rel + ' rc=' + str(r.returncode))
            tail = [l for l in err.splitlines() if l.strip()][-3:]
            for t in tail:
                print('       ' + t[:160])
    except subprocess.TimeoutExpired:
        results['timeout'].append(rel)
        print('TIMEOUT ' + rel)
    except Exception as e:
        results['crash'].append(rel)
        print('CRASH ' + rel + ' ' + str(e)[:100])
print('==== OK=%d FAIL=%d CRASH=%d TIMEOUT=%d ====' % (
    len(results['ok']), len(results['fail']), len(results['crash']), len(results['timeout'])))
