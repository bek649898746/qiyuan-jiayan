# -*- coding: utf-8 -*-
import os, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
GIT = r'C:\Users\Administrator\Desktop\git-2.45.2'
objdir = os.path.join(GIT, '_objs')
CORE_DIRS = ['', 'builtin', 'reftable', 'xdiff', 'ewah', 'refs', 'negotiator',
             'trace2', 'sha1dc', 'sha256', 'block-sha1']
COMPAT_SKIP = {
    'compat\\regex\\regex_internal.c', 'compat\\regex\\regcomp.c', 'compat\\regex\\regexec.c',
    'compat\\win32\\headless.c', 'compat\\msvc.c',
    'compat\\simple-ipc\\ipc-win32.c', 'compat\\simple-ipc\\ipc-shared.c', 'compat\\simple-ipc\\ipc-unix-socket.c',
}
files = []
for d in CORE_DIRS:
    base = os.path.join(GIT, d)
    if os.path.isdir(base):
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
missing = []
for rel in files:
    out = os.path.join(objdir, rel.replace('/', '_').replace('\\', '_')[:-2] + '.o')
    if not os.path.exists(out):
        missing.append(rel)
print('total', len(files), 'missing', len(missing))
for m in missing:
    print('MISSING', m)
