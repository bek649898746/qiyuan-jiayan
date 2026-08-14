# -*- coding: utf-8 -*-
"""Git 全量 -c 对象编译: 用 qcc_x86 -c 把核心+compat 全部 .c 编译成 .o, 供 jyld 全量链接."""
import subprocess, os, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

GIT = r'C:\Users\Administrator\Desktop\git-2.45.2'
QCC = os.environ.get('QCC', r'C:\Users\Administrator\Desktop\qiyuan-jiayan\qcc_x86.exe')
PRE = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\compat_prelude.h'
D = ['-D', '__STDC_VERSION__=199901L', '-D', '__GNUC__=5', '-D', '_WIN32=1', '-D', 'REG_STARTEND=1']

CORE_DIRS = ['', 'builtin', 'reftable', 'xdiff', 'ewah', 'refs', 'negotiator',
             'trace2', 'sha1dc', 'sha256', 'block-sha1']
COMPAT_SKIP = {
    'compat\\regex\\regex_internal.c', 'compat\\regex\\regcomp.c', 'compat\\regex\\regexec.c',
    'compat\\win32\\headless.c',
    'compat\\msvc.c',  # msvc.c 只 #include "mingw.c", 与 mingw.c 重复 (gettimeofday 等)
    'compat\\simple-ipc\\ipc-win32.c',
    'compat\\simple-ipc\\ipc-shared.c', 'compat\\simple-ipc\\ipc-unix-socket.c',
}

files = []
for d in CORE_DIRS:
    base = os.path.join(GIT, d)
    if not os.path.isdir(base):
        continue
    for f in sorted(os.listdir(base)):
        if f.endswith('.c'):
            files.append(os.path.join(d, f).replace('\\', '/'))
for root, dirs, names in os.walk(os.path.join(GIT, 'compat')):
    for f in sorted(names):
        if f.endswith('.c'):
            rel = os.path.relpath(os.path.join(root, f), GIT).replace('\\', '/')
            if rel.replace('/', '\\') in COMPAT_SKIP:
                continue
            files.append(rel)

objdir = os.path.join(GIT, '_objs')
os.makedirs(objdir, exist_ok=True)

ok, fail, crash, timeout = [], [], [], []
for rel in files:
    p = os.path.join(GIT, rel.replace('/', os.sep))
    # .o 命名用完整相对路径 (Git 有 36 组同名 .c: apply.c vs builtin/apply.c, sha1dc/sha1.c vs block-sha1/sha1.c)
    out = os.path.join(objdir, rel.replace('/', '_').replace('\\', '_')[:-2] + '.o')
    if os.path.exists(out):   # 增量：跳过已成功产出的 .o
        ok.append(rel)
        continue
    args = [QCC] + D + ['-I', PRE]
    if rel == 'compat/regex/regex.c':
        args += ['-I', os.path.join(GIT, 'compat', 'regex', 'regex.h')]
        args += ['-D', 'NO_MBSUPPORT']   # regex_internal.h #ifndef NO_MBSUPPORT 保护 mbsupport.h (gawk 头, 不存在)
    args += ['-c', p, '-o', out]
    try:
        r = subprocess.run(args, capture_output=True, timeout=90, cwd=GIT)
        err = (r.stdout + r.stderr).decode('utf-8', 'replace')
        if r.returncode == 0:
            ok.append(rel)
        elif r.returncode < 0:
            crash.append((rel, r.returncode))
        else:
            first = next((l.strip()[:110] for l in err.splitlines() if '[ERR]' in l or 'error' in l.lower()), err.strip()[:110])
            fail.append((rel, first))
    except subprocess.TimeoutExpired:
        timeout.append(rel)

print('=' * 70)
print('qcc -c 全量对象编译 (核心+compat %d 个 .c):' % len(files))
print('  OK      : %d' % len(ok))
print('  FAIL    : %d' % len(fail))
print('  CRASH   : %d' % len(crash))
print('  TIMEOUT : %d' % len(timeout))
print('=' * 70)
for f, rc in crash:
    print('  CRASH %-40s rc=0x%08X' % (f, rc & 0xFFFFFFFF))
for f in timeout:
    print('  TIMEOUT %s' % f)
print('--- FAIL (前 40) ---')
for f, e in fail[:40]:
    print('  %-44s | %s' % (f, e))
if len(fail) > 40:
    print('  ... 共 %d 个失败' % len(fail))
print('--- OK 清单 ---')
for f in ok:
    print('  OK %s' % f)
