# -*- coding: utf-8 -*-
"""Git 全量链接: 470 .o → jyld → git.exe
用法: python scripts/_git_full_link.py
排除有独立 main 的测试工具 (base85.o, _x2.o), common-main.o 提供 main.
"""
import subprocess, os, sys, io, glob
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

GIT = r'C:\Users\Administrator\Desktop\git-2.45.2'
JYLD = r'C:\Users\Administrator\Desktop\qiyuan-jiayan\jyld.exe'
objdir = os.path.join(GIT, '_objs')

# 有独立 main 的测试工具 (不在 git.exe 里)
# regcomp_enhanced.o 与 compat/regex/regex.o 互斥 (git_regcomp 两处定义), Windows 用内置 GNU regex
# compat_stub_procinfo.o 是 stub, Windows 有 compat/win32/trace2_win32_process_info.c 提供
EXCLUDE = {'base85.o', '_x2.o', 'compat_regcomp_enhanced.o', 'compat_stub_procinfo.o',
           'compat_mmap.o',  # mmap.o 是 POSIX, Windows 用 compat_win32mmap.o
           # 独立可执行程序 (各自有 cmd_main, 不链进 git.exe)
           'daemon.o', 'http-backend.o', 'http-fetch.o', 'http-push.o',
           'imap-send.o', 'remote-curl.o', 'scalar.o', 'shell.o', 'sh-i18n--envsubst.o'}

# 非 Windows 平台文件 (darwin/linux 等) — 与 -win32 成对, Windows 构建只链接 win32 变体
PLATFORM_SUFFIXES = ('-darwin.o', '-linux.o', '-macos.o', '-unix.o', '-apple.o')

objs = sorted(f for f in os.listdir(objdir)
              if f.endswith('.o') and f not in EXCLUDE
              and not f.endswith(PLATFORM_SUFFIXES)
              and not f.startswith('compat_linux_'))  # compat/linux/* 是 Linux 平台文件 (Windows 用 stub/win32)
print(f'链接 {len(objs)} 个 .o (排除 {sorted(EXCLUDE)})')

# jyld 命令行 (470 个 .o)
cmd = [JYLD, '-o', os.path.join(GIT, 'git_jiayan.exe')] + [os.path.join(objdir, f) for f in objs]
r = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=300)
print('stdout:', r.stdout.strip()[-2000:])
print('stderr:', r.stderr.strip()[-3000:])
print('rc =', r.returncode)
