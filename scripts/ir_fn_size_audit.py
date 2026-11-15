# -*- coding: utf-8 -*-
"""函数级体积审计：找出 **IR 产物比 legacy 明显短** 的函数（静默丢码的强嫌疑）。

静默丢码（例如 刀647 的捕获泄漏把整个函数体写成 0）不会报错，只会让函数变短。
本脚本按 `.text` 符号表算每个函数的字节长度，两模式对比，列出 `IR < legacy * 0.8`
或 `IR - legacy <= -16` 的函数。

用法: python scripts/ir_fn_size_audit.py --cc build/stU0.exe [--files lvm.c,lstrlib.c] [--min 16]
"""
import argparse
import io
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = r'D:\qcc_corpus\lua-5.4.7\src'
OBJDUMP = r'C:\mingw-w64\bin\objdump.exe'
WORK = os.path.join(REPO, 'build', 'fnsize')


def build(cc, path, out, ir):
    env = dict(os.environ)
    if ir:
        env['QCC_CODEGEN'] = 'ir'
    else:
        env.pop('QCC_CODEGEN', None)
    r = subprocess.run([cc, path, '-c', '-o', out], capture_output=True, env=env, cwd=REPO)
    return r.returncode == 0


def sizes(obj):
    r = subprocess.run([OBJDUMP, '-t', obj], capture_output=True)
    syms = []
    for line in r.stdout.decode('utf-8', 'replace').splitlines():
        m = re.match(r'^\[\s*\d+\]\(sec\s+(\d+)\).*?0x([0-9a-f]+) (\S+)$', line.strip())
        if m and m.group(1) == '1':
            syms.append((int(m.group(2), 16), m.group(3)))
    syms.sort()
    d = {}
    for i, (off, nm) in enumerate(syms):
        end = syms[i + 1][0] if i + 1 < len(syms) else off
        d[nm] = end - off
    return d


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='build/stU0.exe')
    ap.add_argument('--files', default='')
    ap.add_argument('--min', type=int, default=16)
    a = ap.parse_args()
    os.makedirs(WORK, exist_ok=True)
    files = ([x.strip() for x in a.files.split(',') if x.strip()]
             if a.files else sorted(os.path.basename(p) for p in
                                    __import__('glob').glob(os.path.join(SRC, '*.c'))))
    total = 0
    for f in files:
        p = os.path.join(SRC, f)
        lo = os.path.join(WORK, f + '.leg.o')
        ir = os.path.join(WORK, f + '.ir.o')
        if not (build(a.cc, p, lo, False) and build(a.cc, p, ir, True)):
            print('%-14s COMPILE-FAIL' % f)
            continue
        sl, si = sizes(lo), sizes(ir)
        rows = []
        for nm, n in sl.items():
            if nm not in si:
                continue
            d = si[nm] - n
            if n >= 8 and (d <= -a.min or (n >= 32 and d < -0.2 * n)):
                rows.append((d, n, si[nm], nm))
        rows.sort()
        if rows:
            print('== %s' % f)
            for d, n, m, nm in rows[:12]:
                print('   %-32s legacy=%5d ir=%5d (%+d)' % (nm, n, m, d))
            total += len(rows)
    print('可疑函数总数 = %d' % total)


main()
