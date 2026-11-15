# -*- coding: utf-8 -*-
"""SITE 日志对齐比较：按 `码位`（= .text 字节偏移）比较两模式的指令流。

用法:
  python scripts/site_diff.py <leg.log> <ir.log> [--ctx 6] [--fn 名字 --obj xx.o] [--from 偏移]

只比较 `sm=0 cap=0` 的条目（真实发射）。回放跨度不走 `汇发` ⟹ IR 侧 `cp` 会跳空。
函数归属用 objdump 的符号表（.o 的 .text 偏移 == 码位）。
"""
import io
import re
import subprocess
import sys

LINE = re.compile(r'^\[SITE\] cp=(\d+) sm=(\d+) cap=(\d+) ?(.*)$')
OBJDUMP = r'C:\mingw-w64\bin\objdump.exe'


def func_range(obj, name):
    r = subprocess.run([OBJDUMP, '-t', obj], capture_output=True)
    text = r.stdout.decode('utf-8', 'replace')
    syms = []
    for line in text.splitlines():
        m = re.match(r'^\[\s*\d+\]\(sec\s+(\d+)\).*?0x([0-9a-f]+) (\S+)$', line.strip())
        if not m:
            continue
        if m.group(1) != '1':      # sec 1 = .text（pe-x86-64 固定）
            continue
        syms.append((int(m.group(2), 16), m.group(3)))
    syms.sort()
    for i, (off, nm) in enumerate(syms):
        if nm == name:
            end = syms[i + 1][0] if i + 1 < len(syms) else 1 << 30
            return off, end
    return None


def load(path, lo=None, hi=None):
    rows = []
    for line in io.open(path, encoding='utf-8', errors='replace'):
        m = LINE.match(line.rstrip('\n'))
        if not m:
            continue
        cp, sm, cap, txt = int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4)
        if sm or cap:
            continue
        if not txt or txt.startswith(';') or txt.startswith('.L'):
            continue
        if lo is not None and not (lo <= cp < hi):
            continue
        rows.append((cp, txt))
    return rows


def main():
    a_path, b_path = sys.argv[1], sys.argv[2]
    ctx = 6 if '--ctx' not in sys.argv else int(sys.argv[sys.argv.index('--ctx') + 1])
    lo = hi = None
    if '--fn' in sys.argv:
        fn = sys.argv[sys.argv.index('--fn') + 1]
        obj = sys.argv[sys.argv.index('--obj') + 1]
        rg = func_range(obj, fn)
        if not rg:
            print('找不到函数', fn)
            return
        lo, hi = rg
        print('函数 %s 区间 [%d, %d)' % (fn, lo, hi))
    elif '--from' in sys.argv:
        lo = int(sys.argv[sys.argv.index('--from') + 1])
        hi = int(sys.argv[sys.argv.index('--to') + 1]) if '--to' in sys.argv else None
    a = load(a_path, lo, hi)
    b = load(b_path, lo, hi)
    ia = ib = 0
    shown = 0
    n = 0
    while ia < len(a) and ib < len(b):
        if a[ia][0] == b[ib][0] and a[ia][1] == b[ib][1]:
            ia += 1
            ib += 1
            continue
        n += 1
        if shown < ctx:
            print('--- 差异 #%d ---' % n)
            for k in range(max(0, ia - 3), min(len(a), ia + 3)):
                print('  %s L %6d | %s' % ('>>' if k == ia else '  ', a[k][0], a[k][1]))
            for k in range(max(0, ib - 3), min(len(b), ib + 3)):
                print('  %s I %6d | %s' % ('>>' if k == ib else '  ', b[k][0], b[k][1]))
            shown += 1
        ca, cb = a[ia][0], b[ib][0]
        if ca == cb:
            ia += 1
            ib += 1
        elif ca < cb:
            ia += 1
        else:
            ib += 1
    print('合计差异 = %d ；L 条目 %d ；I 条目 %d' % (n, len(a), len(b)))


main()
