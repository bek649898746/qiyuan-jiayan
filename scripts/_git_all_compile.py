# -*- coding: utf-8 -*-
"""Git 全量编译基线: 用 v4 (自宿主) 逐个编译核心 .c, 统计通过/失败/崩溃/超时."""
import subprocess, os, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

GIT = r'C:\Users\Administrator\Desktop\git-2.45.2'
V4 = os.environ.get('QCC', r'C:\Users\Administrator\Desktop\qiyuan-jiayan\v4.exe')
D = ['-D', '__STDC_VERSION__=199901L', '-D', '__GNUC__=5', '-D', '_WIN32=1', '-D', 'REG_STARTEND=1']

# 核心目录 (排除 t/ oss-fuzz/ contrib/ compat/)
CORE_DIRS = ['', 'builtin', 'reftable', 'xdiff', 'ewah', 'refs', 'negotiator',
             'trace2', 'sha1dc', 'sha256', 'block-sha1']

files = []
for d in CORE_DIRS:
    base = os.path.join(GIT, d)
    if not os.path.isdir(base):
        continue
    for f in sorted(os.listdir(base)):
        if f.endswith('.c'):
            files.append(os.path.join(d, f))

ok, fail, crash, timeout = [], [], [], []
for rel in files:
    p = os.path.join(GIT, rel)
    out = os.path.join(GIT, '_all_' + os.path.basename(rel)[:-2] + '.exe')
    try:
        r = subprocess.run([V4] + D + [p, '-o', out], capture_output=True, timeout=90, cwd=GIT)
        err = (r.stdout + r.stderr).decode('utf-8', 'replace')
        if r.returncode == 0:
            ok.append(rel)
        elif r.returncode < 0:
            crash.append((rel, r.returncode))
        else:
            first = next((l.strip()[:110] for l in err.splitlines() if '[ERR]' in l or 'error' in l.lower() or '[WARN]' in l), err.strip()[:110])
            fail.append((rel, first))
    except subprocess.TimeoutExpired:
        timeout.append(rel)
    finally:
        try: os.remove(out)
        except OSError: pass

print('=' * 70)
print('v4 全量编译基线 (核心 %d 个 .c):' % len(files))
print('  OK      : %d' % len(ok))
print('  FAIL    : %d' % len(fail))
print('  CRASH   : %d' % len(crash))
print('  TIMEOUT : %d' % len(timeout))
print('=' * 70)
print('--- CRASH ---')
for f, rc in crash:
    print('  %-40s rc=0x%08X' % (f, rc & 0xFFFFFFFF))
print('--- FAIL (前 40) ---')
for f, e in fail[:40]:
    print('  %-40s | %s' % (f, e))
if len(fail) > 40:
    print('  ... 共 %d 个失败' % len(fail))
