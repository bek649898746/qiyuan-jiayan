# -*- coding: utf-8 -*-
"""Sync seed snapshot files from srclib (per 大哥 directive 2026-08-12).
seed/qcc_x86.c  <- srclib/qcc_x86.c   (host source)
seed/qcc.jy     <- srclib_jiayan/qcc_work.jy  (mirror source)
seed/qcc_rt.c   <- srclib/qcc_rt.c    (runtime, unchanged but sync anyway)
seed/asm_zh.c   <- srclib/asm_zh.c    (assembler source)
seed/jyld.c     <- srclib/jyld.c if exists
"""
import shutil, os

pairs = [
    ('srclib/qcc_x86.c', 'seed/qcc_x86.c'),
    ('srclib_jiayan/qcc_work.jy', 'seed/qcc.jy'),
    ('srclib/qcc_rt.c', 'seed/qcc_rt.c'),
    ('srclib/asm_zh.c', 'seed/asm_zh.c'),
]
for src, dst in pairs:
    if os.path.exists(src):
        shutil.copyfile(src, dst)
        print(f'[OK] {src} -> {dst}')
    else:
        print(f'[SKIP] {src} not found')

# jyld.c - check both locations
for cand in ('srclib/jyld.c', 'srclib_jiayan/jyld.c'):
    if os.path.exists(cand):
        shutil.copyfile(cand, 'seed/jyld.c')
        print(f'[OK] {cand} -> seed/jyld.c')
        break

# verify sizes
for f in ('seed/qcc_x86.c', 'seed/qcc.jy', 'seed/qcc_rt.c', 'seed/asm_zh.c'):
    if os.path.exists(f):
        print(f'{f}: {os.path.getsize(f)} bytes')
