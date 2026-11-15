# -*- coding: utf-8 -*-
"""给定 .c，打印 legacy vs IR 每个函数的差异片段（规范化：去注释、跳转目标归一）。

用法: python scripts/ir_fn_diff.py --cc build/x.exe llex.c [--max 4]
"""
import argparse
import difflib
import io
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = r'D:\qcc_corpus\lua-5.4.7\src'
OBJDUMP = r'C:\mingw-w64\bin\objdump.exe'
WORK = os.path.join(REPO, 'build', 'fndiff')


def comp(cc, path, out, ir):
    env = dict(os.environ)
    if ir:
        env['QCC_CODEGEN'] = 'ir'
    else:
        env.pop('QCC_CODEGEN', None)
    return subprocess.run([cc, path, '-c', '-o', out], capture_output=True, env=env,
                          cwd=REPO).returncode == 0


def fns(path):
    cur = None
    d = {}
    for line in io.open(path, encoding='utf-8', errors='replace'):
        if line[:1] in '0123456789abcdef' and '<' in line:
            cur = line.split('<')[1].split('>')[0]
            d.setdefault(cur, [])
            continue
        if cur and '\t' in line:
            t = line.rstrip().split('\t')[-1].strip().split('#')[0].strip()
            t = re.sub(r'\s[0-9a-f]+ <[^>]*>', ' <T>', t)
            d[cur].append(t)
    return d


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='qcc_zh_v2.exe')
    ap.add_argument('file')
    ap.add_argument('--max', type=int, default=4, help='每函数最多显示几个差异块')
    a = ap.parse_args()
    os.makedirs(WORK, exist_ok=True)
    p = os.path.join(SRC, a.file)
    lo = os.path.join(WORK, a.file + '.l.obj')
    io_ = os.path.join(WORK, a.file + '.i.obj')
    if not comp(a.cc, p, lo, False) or not comp(a.cc, p, io_, True):
        print('COMPILE-FAIL')
        return 1
    for obj, dis in ((lo, lo + '.dis'), (io_, io_ + '.dis')):
        with open(dis, 'wb') as f:
            f.write(subprocess.run([OBJDUMP, '-d', obj], capture_output=True).stdout)
    A = fns(lo + '.dis')
    B = fns(io_ + '.dis')
    total = 0
    for k in A:
        if k not in B or A[k] == B[k]:
            continue
        sm = difflib.SequenceMatcher(None, A[k], B[k], autojunk=False)
        n = sum(max(i2 - i1, j2 - j1) for t, i1, i2, j1, j2 in sm.get_opcodes() if t != 'equal')
        total += n
        print('== %s  差异 %d (L=%d I=%d)' % (k, n, len(A[k]), len(B[k])))
        shown = 0
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            if tag == 'equal':
                continue
            shown += 1
            if shown > a.max:
                print('   ...')
                break
            print('   @%d %s' % (i1, tag))
            for x in A[k][max(0, i1 - 1):i1]:
                print('     ctxL ' + x)
            for x in A[k][i1:i2][:6]:
                print('     L    ' + x)
            for x in B[k][j1:j2][:6]:
                print('     I    ' + x)
    print('合计 %d' % total)
    return 0 if total == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
