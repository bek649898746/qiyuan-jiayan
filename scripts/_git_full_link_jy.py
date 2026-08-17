# -*- coding: utf-8 -*-
"""Git 全量链接 (v1 甲言版): 478 .o → jyld → git_甲言.exe
用法: python scripts/_git_full_link_jy.py
排除有独立 main 的测试工具, common-main.o 提供 main.
"""
import subprocess, os, sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

GIT = r'C:\Users\Administrator\Desktop\git-2.45.2'
JYLD = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\jyld.exe'
objdir = os.path.join(GIT, '_objs_jy')

# 与 C 版 _git_full_link.py 相同的排除集 (有独立 main 的测试工具/平台变体)
EXCLUDE = {'_x2.o', 'compat_regcomp_enhanced.o', 'compat_stub_procinfo.o',
           'compat_mmap.o',
           'daemon.o', 'http-backend.o', 'http-fetch.o', 'http-push.o',
           'imap-send.o', 'remote-curl.o', 'scalar.o', 'shell.o', 'sh-i18n--envsubst.o'}

PLATFORM_SUFFIXES = ('-darwin.o', '-linux.o', '-macos.o', '-unix.o', '-apple.o')

objs = sorted(f for f in os.listdir(objdir)
              if f.endswith('.o') and f not in EXCLUDE
              and not f.endswith(PLATFORM_SUFFIXES)
              and not f.startswith('compat_linux_')
              and f != 'compat_mingw.o'  # mingw.c 被 msvc.c include, 重复符号 (fix 2026-08-17)
              and f != 'compat_simple-ipc_ipc-unix-socket.o')  # 平台变体: win32 优先 (fix 2026-08-17)
print(f'链接 {len(objs)} 个 .o (排除 {sorted(EXCLUDE)})')

cmd = [JYLD, '-o', os.path.join(GIT, 'git_jiayan_v1.exe')] + [os.path.join(objdir, f) for f in objs]
r = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=600)
print('stdout:', r.stdout.strip()[-2000:])
print('stderr:', r.stderr.strip()[-4000:])
print('rc =', r.returncode)
if r.returncode == 0 and os.path.exists(os.path.join(GIT, 'git_jiayan_v1.exe')):
    print('OK:', os.path.getsize(os.path.join(GIT, 'git_jiayan_v1.exe')), 'bytes')
