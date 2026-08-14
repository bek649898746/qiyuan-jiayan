# -*- coding: utf-8 -*-
"""列出缺 .o 的 Git 源文件 (与 _git_all_objects.py 同名映射一致)."""
import os, io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
GIT = r'C:\Users\Administrator\Desktop\git-2.45.2'
CORE_DIRS = ['', 'builtin', 'reftable', 'xdiff', 'ewah', 'refs', 'negotiator',
             'trace2', 'sha1dc', 'sha256', 'block-sha1']
COMPAT_SKIP = {
    'compat\\regex\\regex_internal.c', 'compat\\regex\\regcomp.c', 'compat\\regex\\regexec.c',
    'compat\\mingw.c',
    'compat\\win32\\headless.c',
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
missing = []
for rel in files:
    out = os.path.join(objdir, rel.replace('/', '_').replace('\\', '_')[:-2] + '.o')
    if not os.path.exists(out):
        missing.append(rel)
print('total=%d missing=%d present=%d' % (len(files), len(missing), len(files) - len(missing)))
for m in missing:
    print('MISS ' + m)
