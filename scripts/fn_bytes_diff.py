# -*- coding: utf-8 -*-
"""比较同一函数在两模式下的字节（各自按本对象里的符号定位，按相对偏移对齐）。

用法: python scripts/fn_bytes_diff.py <leg.log> <ir.log> <leg.o> <ir.o> <函数名> [--max N]
"""
import io
import os
import re
import subprocess
import sys

OBJCOPY = r'C:\mingw-w64\bin\objcopy.exe'
OBJDUMP = r'C:\mingw-w64\bin\objdump.exe'
SITE = re.compile(r'^\[SITE\] cp=(\d+) sm=(\d+) cap=(\d+) ?(.*)$')


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


def load(log, lo, hi):
    rows = []
    for line in io.open(log, encoding='utf-8', errors='replace'):
        m = SITE.match(line.rstrip('\n'))
        if not m:
            continue
        cp, sm, cap, txt = int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4)
        if sm or cap or not txt or txt.startswith(';') or txt.startswith('.L'):
            continue
        if lo <= cp < hi:
            rows.append((cp, txt))
    rows.sort()
    return rows


def same(a, b):
    if a == b:
        return True
    if len(a) == len(b) and len(a) >= 5 and a[:-4] == b[:-4]:
        return True
    if len(a) == len(b) and len(a) >= 2 and a[:-1] == b[:-1]:
        return True
    return False


def main():
    a_log, b_log, a_obj, b_obj, fn = sys.argv[1:6]
    mx = 6 if '--max' not in sys.argv else int(sys.argv[sys.argv.index('--max') + 1])
    ra, rb = func_range(a_obj, fn), func_range(b_obj, fn)
    if not ra or not rb:
        print('找不到函数', fn, ra, rb)
        return
    print('L 区间 [%d,%d) 长度 %d ; I 区间 [%d,%d) 长度 %d' %
          (ra[0], ra[1], ra[1] - ra[0], rb[0], rb[1], rb[1] - rb[0]))
    ta, tb = text_bytes(a_obj), text_bytes(b_obj)
    A = load(a_log, *ra)
    n = shown = 0
    for i, (cp, txt) in enumerate(A):
        nxt = A[i + 1][0] if i + 1 < len(A) else ra[1]
        oa = cp - ra[0]
        ob = oa  # 以相对偏移对齐
        aa = ta[cp:nxt]
        bb = tb[rb[0] + ob:rb[0] + ob + len(aa)]
        if same(aa, bb):
            continue
        n += 1
        if shown < mx:
            shown += 1
            print('--- 差异 #%d 相对偏移 %d（cp=%d）%s' % (n, oa, cp, txt))
            print('  >> L [%s]' % aa.hex(' '))
            print('  >> I [%s]' % bb.hex(' '))
    print('合计差异 = %d（legacy 指令 %d 条）' % (n, len(A)))


main()
