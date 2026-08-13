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
EXCLUDE = {'base85.o', '_x2.o'}

objs = sorted(f for f in os.listdir(objdir) if f.endswith('.o') and f not in EXCLUDE)
print(f'链接 {len(objs)} 个 .o (排除 {sorted(EXCLUDE)})')

# jyld 命令行 (470 个 .o)
cmd = [JYLD, '-o', os.path.join(GIT, 'git_jiayan.exe')] + [os.path.join(objdir, f) for f in objs]
r = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=300)
print('stdout:', r.stdout.strip()[-2000:])
print('stderr:', r.stderr.strip()[-3000:])
print('rc =', r.returncode)
