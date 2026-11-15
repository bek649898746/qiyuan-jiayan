# -*- coding: utf-8 -*-
"""函数级字节序列 diff（difflib），给出第一处真实的插入/删除/替换。

用法: python scripts/fn_seq_diff.py <leg.o> <ir.o> <函数名> [--max 6]
"""
import io
import os
import re
import subprocess
import sys
import difflib

OBJCOPY = r'C:\mingw-w64\bin\objcopy.exe'
OBJDUMP = r'C:\mingw-w64\bin\objdump.exe'


def text_bytes(obj):
    out = obj + '.text.bin'
    subprocess.run([OBJCOPY, '--only-section=.text', '-O', 'binary', obj, out], check=True)
    b = io.open(out, 'rb').read()
    os.remove(out)
    return b


def func_range(obj, name):
    r = subprocess.run([OBJDUMP, '-t', obj], capture_output=True)
    syms = []
    for line in r.stdout.decode('utf-8', 'replace').splitlines():
        m = re.match(r'^\[\s*\d+\]\(sec\s+(\d+)\).*?0x([0-9a-f]+) (\S+)$', line.strip())
        if m and m.group(1) == '1':
            syms.append((int(m.group(2), 16), m.group(3)))
    syms.sort()
    for i, (off, nm) in enumerate(syms):
        if nm == name:
            return off, (syms[i + 1][0] if i + 1 < len(syms) else 1 << 30)
    return None


def main():
    a_obj, b_obj, fn = sys.argv[1:4]
    mx = 6 if '--max' not in sys.argv else int(sys.argv[sys.argv.index('--max') + 1])
    ra, rb = func_range(a_obj, fn), func_range(b_obj, fn)
    ta, tb = text_bytes(a_obj), text_bytes(b_obj)
    a = list(ta[ra[0]:ra[1]])
    b = list(tb[rb[0]:rb[1]])
    print('长度 L=%d I=%d（差 %+d）' % (len(a), len(b), len(b) - len(a)))
    sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
    shown = 0
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == 'equal':
            continue
        # 纯 rel32 噪声：两侧长度相同且除 4 字节外一致
        if tag == 'replace' and (i2 - i1) == (j2 - j1):
            x, y = a[i1:i2], b[j1:j2]
            if len(x) >= 5 and x[:-4] == y[:-4]:
                continue
            if len(x) >= 2 and x[:-1] == y[:-1]:
                continue
            if len(x) <= 4:          # 短块 ⟹ 多半落在 rel32/rel8 位移里
                continue
        shown += 1
        print('--- %s  L[%d:%d) I[%d:%d)' % (tag, i1, i2, j1, j2))
        print('    L 前文 %s' % bytes(a[max(0, i1 - 12):i1]).hex(' '))
        print('    L 本段 %s' % bytes(a[i1:min(i2, i1 + 48)]).hex(' '))
        print('    I 本段 %s' % bytes(b[j1:min(j2, j1 + 48)]).hex(' '))
        print('    I 后文 %s' % bytes(b[j2:j2 + 12]).hex(' '))
        if shown >= mx:
            break


main()
